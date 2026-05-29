// EconomicSimulator.cpp
// Компиляция: g++ -std=c++17 EconomicSimulator.cpp -o simulator
// Запуск: ./simulator

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cstdlib>
#include <ctime>

using namespace std;

// Глобальный генератор случайных чисел
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dis_int(0, 100);

// Вспомогательные функции изменения цен
int increasePrice(int price, int percent) {
    if (price >= 10) {
        return max(1, price * percent / 100);
    }
    return price + 1;
}

int decreasePrice(int price, int percent) {
    if (price >= 10) {
        return max(1, price * percent / 100);
    }
    return max(1, price - 1);
}

// ================= БАЗОВЫЙ КЛАСС ЭКОНОМИЧЕСКОГО АГЕНТА =================
class Entity {
public:
    int id;
    string name;
    int money_balance;
    int last_money_balance;

    Entity(int _id, const string& _name, int _money)
        : id(_id), name(_name), money_balance(_money), last_money_balance(_money) {
    }

    virtual ~Entity() {}

    void saveBalance() {
        last_money_balance = money_balance;
    }

    bool wasLastTurnProfitable() const {
        return money_balance >= last_money_balance;
    }
};

// ================= КЛАСС РАБОЧЕГО =================
class Worker : public Entity {
public:
    int goods_stored;
    int employer_type;
    int employer_id;
    int desired_salary;
    int unemployment_counter;

    Worker(int _id, const string& _name)
        : Entity(_id, _name, 20), goods_stored(0),
        employer_type(0), employer_id(-1),
        desired_salary(5 + (dis_int(gen) % 10)),
        unemployment_counter(0) {
    }

    void buyGoods(int price, int max_quantity) {
        if (price <= 0) return;
        int affordable = money_balance / price;
        int quantity = min(max_quantity, affordable);
        if (quantity > 0) {
            int cost = quantity * price;
            money_balance -= cost;
            goods_stored += quantity;
        }
    }

    void earnSalary(int salary) {
        money_balance += salary;
        unemployment_counter = 0;

        if (salary > desired_salary && dis_int(gen) < 30) {
            desired_salary += 1;
        }
        else if (salary < desired_salary && dis_int(gen) < 20 && desired_salary > 3) {
            desired_salary -= 1;
        }
    }

    void adaptUnemployed() {
        unemployment_counter++;

        int probability = 30 + unemployment_counter * 10;
        if (probability > 90) probability = 90;

        if (desired_salary > 2 && dis_int(gen) < probability) {
            desired_salary -= 1;
        }

        if (unemployment_counter >= 5 && desired_salary > 2) {
            desired_salary -= 1;
            unemployment_counter = 0;
        }
    }
};

// ================= КЛАСС ПРОИЗВОДИТЕЛЯ СЫРЬЯ =================
class RawProducer : public Entity {
public:
    int raw_storage;
    int raw_price;
    int salary_rate;
    int max_workers;
    int current_workers;
    int reserved_salary;
    int last_sold;
    int last_produced;
    int production_cost;
    int total_salary_cost;
    bool is_player_controlled;
    int idle_turns;

    RawProducer(int _id, const string& _name, int _money,
        int _raw_price, int _salary, int _max_workers,
        bool _is_player = false)
        : Entity(_id, _name, _money), raw_storage(10), raw_price(_raw_price),
        salary_rate(_salary), max_workers(_max_workers), current_workers(0),
        reserved_salary(0), last_sold(0), last_produced(0),
        production_cost(raw_price), total_salary_cost(0),
        is_player_controlled(_is_player), idle_turns(0) {
    }

    int getAvailableMoney() const {
        return money_balance - reserved_salary;
    }

    void adjust() {
        if (is_player_controlled) return;

        if (last_produced == 0) {
            idle_turns++;
        }
        else {
            idle_turns = 0;
        }

        if (idle_turns >= 3) {
            raw_price = max(1, decreasePrice(raw_price, 70));
            salary_rate = max(3, decreasePrice(salary_rate, 80));

            if (money_balance > 100 && current_workers == 0) {
                salary_rate = increasePrice(salary_rate, 120);
            }
            return;
        }

        if (!wasLastTurnProfitable()) {
            salary_rate = decreasePrice(salary_rate, 90);
            raw_price = max(production_cost, raw_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        if (last_produced == 0) {
            raw_price = max(production_cost, decreasePrice(raw_price, 80));
            salary_rate = decreasePrice(salary_rate, 80);
            raw_price = max(1, raw_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        int sales_ratio = last_sold * 100 / max(1, last_produced);

        if (sales_ratio >= 90) {
            raw_price = increasePrice(raw_price, 120);
            salary_rate = increasePrice(salary_rate, 110);
        }
        else if (sales_ratio >= 70) {
            raw_price = increasePrice(raw_price, 110);
            salary_rate = increasePrice(salary_rate, 110);
        }
        else if (sales_ratio >= 20) {
            raw_price = decreasePrice(raw_price, 90);
            salary_rate = decreasePrice(salary_rate, 90);
        }
        else {
            raw_price = decreasePrice(raw_price, 70);
            salary_rate = decreasePrice(salary_rate, 80);
        }

        if (raw_storage > 50) {
            raw_price = decreasePrice(raw_price, 90);
        }
        else if (raw_storage < 5 && sales_ratio > 80) {
            raw_price = increasePrice(raw_price, 110);
        }

        int estimated_salary_cost = max_workers * salary_rate;
        if (money_balance < estimated_salary_cost && max_workers > 0) {
            salary_rate = max(3, money_balance / max_workers);
        }

        if (money_balance > 150 && current_workers == 0 && idle_turns > 0) {
            salary_rate = increasePrice(salary_rate, 130);
        }

        raw_price = max(production_cost, raw_price);
        raw_price = max(1, raw_price);
        salary_rate = max(3, salary_rate);
    }

    void produce() {
        total_salary_cost = reserved_salary;
        money_balance -= reserved_salary;
        reserved_salary = 0;

        int produced = current_workers * 2;
        raw_storage += produced;
        last_produced = produced;

        if (produced > 0) {
            production_cost = (total_salary_cost + produced - 1) / produced;
            if (production_cost < 1) production_cost = 1;
        }
    }

    void sellRaw(int volume, int price) {
        volume = min(volume, raw_storage);
        if (volume > 0) {
            money_balance += volume * price;
            raw_storage -= volume;
        }
    }
};

// ================= КЛАСС ПРОИЗВОДИТЕЛЯ ТОВАРОВ =================
class GoodProducer : public Entity {
public:
    int raw_storage;
    int goods_storage;
    int goods_price;
    int salary_rate;
    int max_workers;
    int current_workers;
    int reserved_salary;
    int last_sold;
    int last_produced;
    int production_cost;
    int total_raw_cost;
    int total_salary_cost;
    bool is_player_controlled;
    int idle_turns;

    GoodProducer(int _id, const string& _name, int _money,
        int _goods_price, int _salary, int _max_workers,
        bool _is_player = false)
        : Entity(_id, _name, _money), raw_storage(0), goods_storage(10),
        goods_price(_goods_price), salary_rate(_salary),
        max_workers(_max_workers), current_workers(0),
        reserved_salary(0), last_sold(0), last_produced(0),
        production_cost(goods_price), total_raw_cost(0),
        total_salary_cost(0), is_player_controlled(_is_player),
        idle_turns(0) {
    }

    int getAvailableMoney() const {
        return money_balance - reserved_salary;
    }

    void adjust() {
        if (is_player_controlled) return;

        if (last_produced == 0) {
            idle_turns++;
        }
        else {
            idle_turns = 0;
        }

        if (idle_turns >= 3) {
            goods_price = max(1, decreasePrice(goods_price, 70));
            salary_rate = max(3, decreasePrice(salary_rate, 80));

            if (money_balance > 100 && current_workers == 0) {
                salary_rate = increasePrice(salary_rate, 120);
            }
            return;
        }

        if (!wasLastTurnProfitable()) {
            salary_rate = decreasePrice(salary_rate, 90);
            goods_price = max(production_cost, goods_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        if (last_produced == 0) {
            goods_price = max(production_cost, decreasePrice(goods_price, 80));
            salary_rate = decreasePrice(salary_rate, 80);
            goods_price = max(1, goods_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        int sales_ratio = last_sold * 100 / max(1, last_produced);

        if (sales_ratio >= 90) {
            goods_price = increasePrice(goods_price, 120);
            salary_rate = increasePrice(salary_rate, 110);
        }
        else if (sales_ratio >= 70) {
            goods_price = increasePrice(goods_price, 110);
            salary_rate = increasePrice(salary_rate, 110);
        }
        else if (sales_ratio >= 20) {
            goods_price = decreasePrice(goods_price, 90);
            salary_rate = decreasePrice(salary_rate, 90);
        }
        else {
            goods_price = decreasePrice(goods_price, 70);
            salary_rate = decreasePrice(salary_rate, 80);
        }

        if (goods_storage > 50) {
            goods_price = decreasePrice(goods_price, 90);
        }
        else if (goods_storage < 5 && sales_ratio > 80) {
            goods_price = increasePrice(goods_price, 110);
        }

        if (raw_storage < 5) {
            goods_price = increasePrice(goods_price, 110);
        }
        else if (raw_storage > 30) {
            goods_price = decreasePrice(goods_price, 95);
        }

        int estimated_salary_cost = max_workers * salary_rate;
        if (money_balance < estimated_salary_cost && max_workers > 0) {
            salary_rate = max(3, money_balance / max_workers);
        }

        if (money_balance > 150 && current_workers == 0 && idle_turns > 0) {
            salary_rate = increasePrice(salary_rate, 130);
        }

        goods_price = max(production_cost, goods_price);
        goods_price = max(1, goods_price);
        salary_rate = max(3, salary_rate);
    }

    void produce() {
        total_salary_cost = reserved_salary;
        money_balance -= reserved_salary;
        reserved_salary = 0;

        int raw_needed = current_workers * 2;
        int raw_used = min(raw_needed, raw_storage);
        raw_storage -= raw_used;
        goods_storage += raw_used;
        last_produced = raw_used;

        if (raw_used > 0) {
            int total_raw_available = raw_storage + raw_used;
            int avg_raw_cost = total_raw_available > 0 ?
                total_raw_cost / max(1, total_raw_available) : 1;

            int total_cost = (raw_used * avg_raw_cost) + total_salary_cost;
            production_cost = (total_cost + raw_used - 1) / raw_used;
            if (production_cost < 1) production_cost = 1;

            total_raw_cost = max(0, total_raw_cost - (raw_used * avg_raw_cost));
        }
    }

    void buyRaw(int volume, int price) {
        int cost = volume * price;
        if (getAvailableMoney() >= cost) {
            money_balance -= cost;
            raw_storage += volume;
            total_raw_cost += cost;
        }
    }

    void sellGoods(int volume, int price) {
        volume = min(volume, goods_storage);
        if (volume > 0) {
            money_balance += volume * price;
            goods_storage -= volume;
        }
    }
};

// ================= ГЛАВНЫЙ КЛАСС ЭКОНОМИЧЕСКОГО СИМУЛЯТОРА =================
class EconomicSimulator {
private:
    vector<Worker> workers;
    vector<RawProducer> raw_producers;
    vector<GoodProducer> good_producers;
    int current_turn;

    ofstream log_file;
    ofstream market_log;
    ofstream producer_log;

    int player_company_type;
    int player_company_id;

public:
    EconomicSimulator() : current_turn(0), player_company_type(0), player_company_id(-1) {
        log_file.open("simulation_log.txt");
        market_log.open("market_log.txt");
        producer_log.open("producer_log.txt");
    }

    ~EconomicSimulator() {
        if (log_file.is_open()) log_file.close();
        if (market_log.is_open()) market_log.close();
        if (producer_log.is_open()) producer_log.close();
    }

    void initialize() {
        cout << "\n========================================" << endl;
        cout << "    ЭКОНОМИЧЕСКИЙ СИМУЛЯТОР" << endl;
        cout << "    Выбор режима управления" << endl;
        cout << "========================================" << endl;
        cout << "1. Играть за производителя сырья (RawCorp_1)" << endl;
        cout << "2. Играть за производителя товаров (GoodCorp_1)" << endl;
        cout << "3. Режим наблюдателя (без управления)" << endl;
        cout << "----------------------------------------" << endl;
        cout << "Ваш выбор: ";

        int choice;
        cin >> choice;

        for (int i = 0; i < 12; i++) {
            workers.emplace_back(i + 1, "Worker_" + to_string(i + 1));
        }

        if (choice == 1) {
            raw_producers.emplace_back(1, "RawCorp_1 (ВЫ)", 200, 10, 10, 2, true);
            for (int i = 2; i <= 3; i++) {
                raw_producers.emplace_back(i, "RawCorp_" + to_string(i), 200,
                    8 + (dis_int(gen) % 5), 8 + (dis_int(gen) % 5), 2, false);
            }
            for (int i = 1; i <= 3; i++) {
                good_producers.emplace_back(i, "GoodCorp_" + to_string(i), 200,
                    20 + (dis_int(gen) % 11), 10 + (dis_int(gen) % 6), 2, false);
            }
            player_company_type = 1;
            player_company_id = 1;
            cout << "\nВы управляете компанией RawCorp_1 (производитель сырья)!" << endl;
        }
        else if (choice == 2) {
            for (int i = 1; i <= 3; i++) {
                raw_producers.emplace_back(i, "RawCorp_" + to_string(i), 200,
                    8 + (dis_int(gen) % 5), 8 + (dis_int(gen) % 5), 2, false);
            }
            good_producers.emplace_back(1, "GoodCorp_1 (ВЫ)", 200, 25, 12, 2, true);
            for (int i = 2; i <= 3; i++) {
                good_producers.emplace_back(i, "GoodCorp_" + to_string(i), 200,
                    20 + (dis_int(gen) % 11), 10 + (dis_int(gen) % 6), 2, false);
            }
            player_company_type = 2;
            player_company_id = 1;
            cout << "\nВы управляете компанией GoodCorp_1 (производитель товаров)!" << endl;
        }
        else {
            for (int i = 0; i < 3; i++) {
                raw_producers.emplace_back(i + 1, "RawCorp_" + to_string(i + 1),
                    100, 8 + (dis_int(gen) % 5), 8 + (dis_int(gen) % 5), 2);
            }
            for (int i = 0; i < 3; i++) {
                good_producers.emplace_back(i + 1, "GoodCorp_" + to_string(i + 1),
                    100, 20 + (dis_int(gen) % 11), 10 + (dis_int(gen) % 6), 2);
            }
            cout << "\nРежим наблюдателя. Все компании управляются ИИ." << endl;
        }

        logToFile("=== СИМУЛЯЦИЯ НАЧАТА ===");
        logToFile("Создано рабочих: " + to_string(workers.size()));
        logToFile("Создано производителей сырья: " + to_string(raw_producers.size()));
        logToFile("Создано производителей товаров: " + to_string(good_producers.size()));

        cout << "\nНажмите Enter для продолжения...";
        cin.ignore();
        cin.get();
    }

    void logToFile(const string& msg) {
        if (log_file.is_open()) {
            log_file << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    void logMarket(const string& msg) {
        if (market_log.is_open()) {
            market_log << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    void logProducer(const string& msg) {
        if (producer_log.is_open()) {
            producer_log << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    void saveAllBalances() {
        for (auto& w : workers) w.saveBalance();
        for (auto& r : raw_producers) r.saveBalance();
        for (auto& g : good_producers) g.saveBalance();
    }

    void updateLaborDisplay() {
        for (auto& r : raw_producers) {
            r.current_workers = 0;
        }
        for (auto& g : good_producers) {
            g.current_workers = 0;
        }
        for (auto& w : workers) {
            w.employer_type = 0;
            w.employer_id = -1;
        }

        struct Offer {
            int salary;
            int type;
            int id;
        };
        vector<Offer> offers;

        for (auto& r : raw_producers) {
            int available_money = r.money_balance * 90 / 100;
            int affordable_workers = available_money / max(1, r.salary_rate);
            int workers_to_hire = min(r.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ r.salary_rate, 1, r.id });
            }
        }

        for (auto& g : good_producers) {
            int available_money = g.money_balance * 90 / 100;
            int affordable_workers = available_money / max(1, g.salary_rate);
            int workers_to_hire = min(g.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ g.salary_rate, 2, g.id });
            }
        }

        sort(offers.begin(), offers.end(),
            [](const Offer& a, const Offer& b) { return a.salary > b.salary; });

        for (auto& offer : offers) {
            for (auto& w : workers) {
                if (w.employer_id != -1) continue;

                if (offer.salary >= w.desired_salary) {
                    if (offer.type == 1) {
                        auto& r = raw_producers[offer.id - 1];
                        if (r.current_workers < r.max_workers) {
                            r.current_workers++;
                            w.employer_type = 1;
                            w.employer_id = r.id;
                            break;
                        }
                    }
                    else {
                        auto& g = good_producers[offer.id - 1];
                        if (g.current_workers < g.max_workers) {
                            g.current_workers++;
                            w.employer_type = 2;
                            w.employer_id = g.id;
                            break;
                        }
                    }
                }
            }
        }
    }

    void playerControlMenu() {
        if (player_company_type == 1) {
            controlRawProducer();
        }
        else if (player_company_type == 2) {
            controlGoodProducer();
        }
    }

    void controlRawProducer() {
        auto& player = raw_producers[player_company_id - 1];

        cout << "\n========================================" << endl;
        cout << "  УПРАВЛЕНИЕ КОМПАНИЕЙ " << player.name << endl;
        cout << "========================================" << endl;
        cout << "Баланс: " << player.money_balance;
        cout << " | Сырьё на складе: " << player.raw_storage << endl;
        cout << "Текущая цена сырья: " << player.raw_price;
        cout << " | Зарплата: " << player.salary_rate << endl;
        cout << "Себестоимость единицы: " << player.production_cost << endl;
        cout << "Рабочих: " << player.current_workers;
        cout << "/" << player.max_workers << endl;
        cout << "Продано в прошлом ходу: " << player.last_sold;
        cout << " | Произведено: " << player.last_produced << endl;

        cout << "\n--- Конкуренты (производители сырья) ---" << endl;
        for (auto& r : raw_producers) {
            if (!r.is_player_controlled) {
                cout << "  " << r.name << ": цена=" << r.raw_price
                    << ", запас=" << r.raw_storage
                    << ", рабочих=" << r.current_workers << "/" << r.max_workers << endl;
            }
        }

        cout << "\n--- Покупатели (производители товаров) ---" << endl;
        for (auto& g : good_producers) {
            cout << "  " << g.name << ": баланс=" << g.money_balance
                << ", запас сырья=" << g.raw_storage << endl;
        }

        cout << "\n--- Рынок труда ---" << endl;
        for (auto& w : workers) {
            cout << "  " << w.name << ": баланс=" << w.money_balance
                << ", желаемая ЗП=" << w.desired_salary << endl;
        }

        cout << "\n--- Действия ---" << endl;
        cout << "1. Изменить цену сырья" << endl;
        cout << "2. Изменить зарплату" << endl;
        cout << "3. Ничего не менять" << endl;
        cout << "Ваш выбор: ";

        int choice;
        cin >> choice;

        switch (choice) {
        case 1: {
            cout << "Текущая цена: " << player.raw_price
                << " (себестоимость: " << player.production_cost << ")" << endl;
            cout << "Цены конкурентов: ";
            for (auto& r : raw_producers) {
                if (!r.is_player_controlled) {
                    cout << r.name << "=" << r.raw_price << " ";
                }
            }
            cout << endl;
            cout << "Новая цена: ";
            int new_price;
            cin >> new_price;

            if (new_price < player.production_cost) {
                cout << "ВНИМАНИЕ: Цена ниже себестоимости! ";
                cout << "Установлена минимальная цена = себестоимость." << endl;
                new_price = player.production_cost;
            }
            if (new_price < 1) new_price = 1;
            player.raw_price = new_price;
            break;
        }
        case 2: {
            cout << "Текущая зарплата: " << player.salary_rate << endl;
            cout << "Зарплаты конкурентов: ";
            for (auto& r : raw_producers) {
                if (!r.is_player_controlled) {
                    cout << r.name << "=" << r.salary_rate << " ";
                }
            }
            cout << endl;
            cout << "Новая зарплата: ";
            int new_salary;
            cin >> new_salary;
            if (new_salary < 1) new_salary = 1;
            player.salary_rate = new_salary;
            break;
        }
        case 3:
            cout << "Настройки сохранены без изменений." << endl;
            break;
        default:
            cout << "Неверный выбор. Настройки не изменены." << endl;
        }
    }

    void controlGoodProducer() {
        auto& player = good_producers[player_company_id - 1];

        cout << "\n========================================" << endl;
        cout << "  УПРАВЛЕНИЕ КОМПАНИЕЙ " << player.name << endl;
        cout << "========================================" << endl;
        cout << "Баланс: " << player.money_balance;
        cout << " | Товары на складе: " << player.goods_storage << endl;
        cout << "Сырьё на складе: " << player.raw_storage << endl;
        cout << "Текущая цена товаров: " << player.goods_price;
        cout << " | Зарплата: " << player.salary_rate << endl;
        cout << "Себестоимость единицы: " << player.production_cost << endl;
        cout << "Рабочих: " << player.current_workers;
        cout << "/" << player.max_workers << endl;
        cout << "Продано в прошлом ходу: " << player.last_sold;
        cout << " | Произведено: " << player.last_produced << endl;

        cout << "\n--- Конкуренты (производители товаров) ---" << endl;
        for (auto& g : good_producers) {
            if (!g.is_player_controlled) {
                cout << "  " << g.name << ": цена=" << g.goods_price
                    << ", запас=" << g.goods_storage
                    << ", рабочих=" << g.current_workers << "/" << g.max_workers << endl;
            }
        }

        cout << "\n--- Поставщики (производители сырья) ---" << endl;
        for (auto& r : raw_producers) {
            cout << "  " << r.name << ": цена=" << r.raw_price
                << ", запас=" << r.raw_storage << endl;
        }

        cout << "\n--- Рынок труда ---" << endl;
        for (auto& w : workers) {
            cout << "  " << w.name << ": баланс=" << w.money_balance
                << ", желаемая ЗП=" << w.desired_salary << endl;
        }

        cout << "\n--- Действия ---" << endl;
        cout << "1. Изменить цену товаров" << endl;
        cout << "2. Изменить зарплату" << endl;
        cout << "3. Ничего не менять" << endl;
        cout << "Ваш выбор: ";

        int choice;
        cin >> choice;

        switch (choice) {
        case 1: {
            cout << "Текущая цена: " << player.goods_price
                << " (себестоимость: " << player.production_cost << ")" << endl;
            cout << "Цены конкурентов: ";
            for (auto& g : good_producers) {
                if (!g.is_player_controlled) {
                    cout << g.name << "=" << g.goods_price << " ";
                }
            }
            cout << endl;
            cout << "Новая цена: ";
            int new_price;
            cin >> new_price;

            if (new_price < player.production_cost) {
                cout << "ВНИМАНИЕ: Цена ниже себестоимости! ";
                cout << "Установлена минимальная цена = себестоимость." << endl;
                new_price = player.production_cost;
            }
            if (new_price < 1) new_price = 1;
            player.goods_price = new_price;
            break;
        }
        case 2: {
            cout << "Текущая зарплата: " << player.salary_rate << endl;
            cout << "Зарплаты конкурентов: ";
            for (auto& g : good_producers) {
                if (!g.is_player_controlled) {
                    cout << g.name << "=" << g.salary_rate << " ";
                }
            }
            cout << endl;
            cout << "Новая зарплата: ";
            int new_salary;
            cin >> new_salary;
            if (new_salary < 1) new_salary = 1;
            player.salary_rate = new_salary;
            break;
        }
        case 3:
            cout << "Настройки сохранены без изменений." << endl;
            break;
        default:
            cout << "Неверный выбор. Настройки не изменены." << endl;
        }
    }

    void phase1_SetPricesAndSalaries() {
        logToFile("\n=== ФАЗА 1: Установка цен и зарплат ===");

        updateLaborDisplay();

        if (player_company_type > 0) {
            playerControlMenu();
        }

        for (auto& r : raw_producers) {
            if (!r.is_player_controlled) {
                r.adjust();
            }
            logProducer(r.name + ": цена=" + to_string(r.raw_price) +
                ", зарплата=" + to_string(r.salary_rate) +
                ", баланс=" + to_string(r.money_balance) +
                ", продано/произведено=" + to_string(r.last_sold) +
                "/" + to_string(r.last_produced));
        }

        for (auto& g : good_producers) {
            if (!g.is_player_controlled) {
                g.adjust();
            }
            logProducer(g.name + ": цена=" + to_string(g.goods_price) +
                ", зарплата=" + to_string(g.salary_rate) +
                ", баланс=" + to_string(g.money_balance) +
                ", продано/произведено=" + to_string(g.last_sold) +
                "/" + to_string(g.last_produced));
        }
    }

    void phase2_LaborMarket() {
        logToFile("\n=== ФАЗА 2: Рынок труда ===");

        for (auto& r : raw_producers) {
            r.current_workers = 0;
            r.reserved_salary = 0;
        }
        for (auto& g : good_producers) {
            g.current_workers = 0;
            g.reserved_salary = 0;
        }
        for (auto& w : workers) {
            w.employer_type = 0;
            w.employer_id = -1;
        }

        struct Offer {
            int salary;
            int type;
            int id;
        };
        vector<Offer> offers;

        for (auto& r : raw_producers) {
            int available_money = r.money_balance * 90 / 100;
            int affordable_workers = available_money / max(1, r.salary_rate);
            int workers_to_hire = min(r.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ r.salary_rate, 1, r.id });
            }
        }

        for (auto& g : good_producers) {
            int available_money = g.money_balance * 90 / 100;
            int affordable_workers = available_money / max(1, g.salary_rate);
            int workers_to_hire = min(g.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ g.salary_rate, 2, g.id });
            }
        }

        sort(offers.begin(), offers.end(),
            [](const Offer& a, const Offer& b) { return a.salary > b.salary; });

        int hired_count = 0;

        for (auto& offer : offers) {
            for (auto& w : workers) {
                if (w.employer_id != -1) continue;

                if (offer.salary >= w.desired_salary) {
                    if (offer.type == 1) {
                        auto& r = raw_producers[offer.id - 1];
                        if (r.current_workers < r.max_workers) {
                            int new_reserved = r.reserved_salary + r.salary_rate;
                            if (r.money_balance >= new_reserved) {
                                r.current_workers++;
                                r.reserved_salary = new_reserved;
                                w.employer_type = 1;
                                w.employer_id = r.id;
                                hired_count++;
                                break;
                            }
                        }
                    }
                    else {
                        auto& g = good_producers[offer.id - 1];
                        if (g.current_workers < g.max_workers) {
                            int new_reserved = g.reserved_salary + g.salary_rate;
                            if (g.money_balance >= new_reserved) {
                                g.current_workers++;
                                g.reserved_salary = new_reserved;
                                w.employer_type = 2;
                                w.employer_id = g.id;
                                hired_count++;
                                break;
                            }
                        }
                    }
                }
            }
        }

        logToFile("Нанято рабочих: " + to_string(hired_count) +
            " из " + to_string(workers.size()));
    }

    void phase3_Trade() {
        logToFile("\n=== ФАЗА 3: Торговля ===");

        for (auto& r : raw_producers) r.last_sold = 0;
        for (auto& g : good_producers) g.last_sold = 0;

        logMarket("--- Рынок сырья (B2B) ---");

        for (auto& g : good_producers) {
            int need = max(0, 10 - g.raw_storage);

            vector<pair<int, RawProducer*>> sorted_sellers;
            for (auto& r : raw_producers) {
                if (r.raw_storage > 0) {
                    sorted_sellers.push_back({ r.raw_price, &r });
                }
            }
            sort(sorted_sellers.begin(), sorted_sellers.end());

            for (auto& [price, r] : sorted_sellers) {
                if (need <= 0) break;
                if (r->raw_price <= 0) continue;

                int available_money = g.getAvailableMoney();
                int max_affordable = available_money / r->raw_price;
                int buy = min({ need, r->raw_storage, max_affordable });

                if (buy <= 0) continue;

                int cost = buy * r->raw_price;
                if (g.getAvailableMoney() >= cost) {
                    g.buyRaw(buy, r->raw_price);
                    r->sellRaw(buy, r->raw_price);
                    r->last_sold += buy;
                    need -= buy;

                    logMarket(g.name + " купил " + to_string(buy) +
                        " сырья у " + r->name + " по цене " +
                        to_string(r->raw_price));
                }
            }
        }

        logMarket("--- Рынок товаров (B2C) ---");

        for (auto& w : workers) {
            vector<pair<int, GoodProducer*>> sorted_sellers;
            for (auto& g : good_producers) {
                if (g.goods_storage > 0) {
                    sorted_sellers.push_back({ g.goods_price, &g });
                }
            }
            sort(sorted_sellers.begin(), sorted_sellers.end());

            for (auto& [price, g] : sorted_sellers) {
                if (g->goods_price <= 0) continue;

                int max_affordable = w.money_balance / g->goods_price;
                int buy = min(g->goods_storage, max_affordable);

                if (buy <= 0) continue;

                w.buyGoods(g->goods_price, buy);
                g->sellGoods(buy, g->goods_price);
                g->last_sold += buy;

                logMarket(w.name + " купил " + to_string(buy) +
                    " товаров у " + g->name + " по цене " +
                    to_string(g->goods_price));
            }
        }
    }

    void phase4_Production() {
        logToFile("\n=== ФАЗА 4: Производство и выплата зарплат ===");

        for (auto& r : raw_producers) {
            if (r.current_workers > 0) {
                r.produce();
                logToFile(r.name + " произвёл " + to_string(r.last_produced) +
                    " сырья, рабочих: " + to_string(r.current_workers) +
                    ", баланс: " + to_string(r.money_balance) +
                    ", запас: " + to_string(r.raw_storage));
            }
            else {
                r.last_produced = 0;
                logToFile(r.name + " не производил - нет рабочих");
            }
        }

        for (auto& g : good_producers) {
            if (g.current_workers > 0) {
                g.produce();
                logToFile(g.name + " произвёл " + to_string(g.last_produced) +
                    " товаров, рабочих: " + to_string(g.current_workers) +
                    ", баланс: " + to_string(g.money_balance) +
                    ", запас товаров: " + to_string(g.goods_storage) +
                    ", запас сырья: " + to_string(g.raw_storage));
            }
            else {
                g.last_produced = 0;
                logToFile(g.name + " не производил - нет рабочих");
            }
        }

        int unemployed = 0;
        for (auto& w : workers) {
            if (w.employer_type == 1) {
                w.earnSalary(raw_producers[w.employer_id - 1].salary_rate);
            }
            else if (w.employer_type == 2) {
                w.earnSalary(good_producers[w.employer_id - 1].salary_rate);
            }
            else {
                unemployed++;
                w.adaptUnemployed();
            }
        }

        logToFile("Безработных: " + to_string(unemployed));
    }

    void printStatistics() {
        cout << "\n========================================" << endl;
        cout << "       СТАТИСТИКА ХОДА " << current_turn << endl;
        cout << "========================================" << endl;

        int total_worker_money = 0, total_worker_goods = 0, employed = 0;
        int avg_desired_salary = 0;

        for (auto& w : workers) {
            total_worker_money += w.money_balance;
            total_worker_goods += w.goods_stored;
            avg_desired_salary += w.desired_salary;
            if (w.employer_id != -1) employed++;
        }

        cout << "\n--- Рабочие ---" << endl;
        cout << "  Средний баланс: " << total_worker_money / (int)workers.size() << endl;
        cout << "  Среднее количество товаров: " << total_worker_goods / (int)workers.size() << endl;
        cout << "  Занятость: " << employed << "/" << workers.size() << endl;
        cout << "  Средняя желаемая ЗП: " << avg_desired_salary / (int)workers.size() << endl;

        cout << "\n--- Производители сырья ---" << endl;
        for (auto& r : raw_producers) {
            cout << "  " << r.name << ": деньги=" << r.money_balance
                << ", сырьё=" << r.raw_storage
                << ", рабочих=" << r.current_workers << "/" << r.max_workers
                << ", цена=" << r.raw_price
                << ", продано=" << r.last_sold << endl;
        }

        cout << "\n--- Производители товаров ---" << endl;
        for (auto& g : good_producers) {
            cout << "  " << g.name << ": деньги=" << g.money_balance
                << ", товары=" << g.goods_storage
                << ", сырьё=" << g.raw_storage
                << ", рабочих=" << g.current_workers << "/" << g.max_workers
                << ", цена=" << g.goods_price
                << ", продано=" << g.last_sold << endl;
        }
        cout << "========================================" << endl;
    }

    void runSimulation(int turns) {
        for (int t = 1; t <= turns; t++) {
            current_turn = t;
            cout << "\n+----------------------------------------+" << endl;
            cout << "|              ХОД " << t << "                     |" << endl;
            cout << "+----------------------------------------+" << endl;

            saveAllBalances();
            phase1_SetPricesAndSalaries();
            phase2_LaborMarket();
            phase3_Trade();
            phase4_Production();
            printStatistics();

            if (t < turns) {
                cout << "\nНажмите Enter для продолжения...";
                cin.ignore();
                cin.get();
            }
        }

        logToFile("=== СИМУЛЯЦИЯ ЗАВЕРШЕНА ===");
        cout << "\n========================================" << endl;
        cout << "       СИМУЛЯЦИЯ ЗАВЕРШЕНА" << endl;
        cout << "========================================" << endl;
        cout << "Логи сохранены в файлы:" << endl;
        cout << "  - simulation_log.txt" << endl;
        cout << "  - market_log.txt" << endl;
        cout << "  - producer_log.txt" << endl;
        cout << "========================================" << endl;
    }

    void showAllAgents() {
        cout << "\n========================================" << endl;
        cout << "    СОСТОЯНИЕ ВСЕХ АГЕНТОВ" << endl;
        cout << "========================================" << endl;

        cout << "\n--- Рабочие ---" << endl;
        for (auto& w : workers) {
            cout << "ID:" << w.id << " " << w.name
                << " | Баланс:" << w.money_balance
                << " | Товары:" << w.goods_stored
                << " | Желаемая ЗП:" << w.desired_salary
                << " | Без работы:" << w.unemployment_counter << " ходов";
            if (w.employer_id != -1) {
                cout << " | Работает в:" << (w.employer_type == 1 ? "Сырьевая" : "Товарная")
                    << " ID:" << w.employer_id;
            }
            else {
                cout << " | Безработный";
            }
            cout << endl;
        }

        cout << "\n--- Производители сырья ---" << endl;
        for (auto& r : raw_producers) {
            cout << "ID:" << r.id << " " << r.name
                << " | Баланс:" << r.money_balance
                << " | Сырьё:" << r.raw_storage
                << " | Цена:" << r.raw_price
                << " | Зарплата:" << r.salary_rate
                << " | Рабочих:" << r.current_workers << "/" << r.max_workers
                << " | Себест.:" << r.production_cost
                << " | Продано:" << r.last_sold
                << " | Простой:" << r.idle_turns << " ходов" << endl;
        }

        cout << "\n--- Производители товаров ---" << endl;
        for (auto& g : good_producers) {
            cout << "ID:" << g.id << " " << g.name
                << " | Баланс:" << g.money_balance
                << " | Сырьё:" << g.raw_storage
                << " | Товары:" << g.goods_storage
                << " | Цена:" << g.goods_price
                << " | Зарплата:" << g.salary_rate
                << " | Рабочих:" << g.current_workers << "/" << g.max_workers
                << " | Себест.:" << g.production_cost
                << " | Продано:" << g.last_sold
                << " | Простой:" << g.idle_turns << " ходов" << endl;
        }
        cout << "========================================" << endl;
    }

    void interactiveMode() {
        cout << "\n========================================" << endl;
        cout << "    ИНТЕРАКТИВНЫЙ РЕЖИМ" << endl;
        cout << "========================================" << endl;

        while (true) {
            cout << "\n--- Главное меню ---" << endl;
            cout << "1. Запустить симуляцию на N ходов" << endl;
            cout << "2. Показать состояние всех агентов" << endl;
            cout << "3. Выйти из программы" << endl;
            cout << "Ваш выбор: ";

            int choice;
            cin >> choice;

            switch (choice) {
            case 1: {
                cout << "Введите количество ходов: ";
                int turns;
                cin >> turns;

                if (turns <= 0) {
                    cout << "Ошибка: количество ходов должно быть положительным!" << endl;
                    break;
                }

                runSimulation(turns);
                break;
            }
            case 2:
                showAllAgents();
                break;
            case 3:
                cout << "\nЗавершение программы..." << endl;
                return;
            default:
                cout << "Неверный выбор. Попробуйте снова." << endl;
            }
        }
    }
};

int main() {
    setlocale(LC_ALL, "Russian");

    cout << "+------------------------------------------------------+" << endl;
    cout << "|        ЭКОНОМИЧЕСКИЙ СИМУЛЯТОР                    |" << endl;
    cout << "|        Версия: 1.1                                |" << endl;
    cout << "+------------------------------------------------------+" << endl;
    cout << endl;
    cout << "Особенности симулятора:" << endl;
    cout << "  * Замкнутая экономическая система" << endl;
    cout << "  * Три типа агентов: рабочие, производители сырья и товаров" << endl;
    cout << "  * Двухсекторная модель производства" << endl;
    cout << "  * Динамическое ценообразование на основе спроса и предложения" << endl;
    cout << "  * Рынок труда с учётом желаемых зарплат рабочих" << endl;
    cout << "  * Адаптивные ИИ-стратегии конкурентов" << endl;
    cout << "  * Интерактивное управление компанией" << endl;
    cout << "  * Детальное логгирование всех событий" << endl;

    EconomicSimulator simulator;
    simulator.initialize();
    simulator.interactiveMode();

    cout << "\nСпасибо за использование экономического симулятора!" << endl;
    return 0;
}
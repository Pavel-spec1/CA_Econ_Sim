// EconomicSimulator.cpp
// Компиляция: g++ -std=c++17 EconomicSimulator.cpp -o simulator
// Запуск: ./simulator

// ============================================================================
// ТЗ п.6: Применяемые технологии
// Разработка выполнена на языке C++ с использованием стандарта C++17,
// русского языка в интерфейсе и приёмов ООП.
// ============================================================================

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
// ТЗ п.3.2.2: Случайное перемешивание очередей для равных возможностей
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dis_int(0, 100);

// ============================================================================
// ТЗ п.3.3: Вспомогательные функции ценообразования
// Для значений от 10 и выше: изменение рассчитывается как процент
// Для значений менее 10: применяется абсолютное изменение
// ============================================================================
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

// ============================================================================
// ТЗ п.2: Описание предметной области — базовый класс агента
// Все экономические агенты имеют имя, денежный баланс
// ============================================================================
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

    // ТЗ п.3.2.1: Оценка прибыльности предыдущего хода
    void saveBalance() {
        last_money_balance = money_balance;
    }

    // ТЗ п.3.2.1: Проверка прибыльности для антикризисного управления
    bool wasLastTurnProfitable() const {
        return money_balance >= last_money_balance;
    }
};

// ============================================================================
// ТЗ п.2: Рабочий — представляет рабочую силу в экономике
// Параметры: имя, денежный баланс, количество накопленных товаров
// Цель: максимизировать количество накопленных товаров
// ============================================================================
class Worker : public Entity {
public:
    int goods_stored;              // ТЗ п.2: количество накопленных товаров
    int employer_type;             // Тип работодателя: 0 - безработный, 1 - сырьевая, 2 - товарная
    int employer_id;               // ID работодателя
    int desired_salary;            // ТЗ п.4.3.4: желаемый уровень зарплаты (динамический)
    int unemployment_counter;      // ТЗ п.3.2.4: счётчик безработицы для адаптации

    Worker(int _id, const string& _name)
        : Entity(_id, _name, 20), goods_stored(0),
        employer_type(0), employer_id(-1),
        desired_salary(5 + (dis_int(gen) % 10)),
        unemployment_counter(0) {
    }

    // ТЗ п.3.4.3: Потребительское поведение — приобретение товаров
    // у производителя с минимальной ценой
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

    // ТЗ п.3.2.4: Выплата зарплаты и адаптация ожиданий рабочего
    void earnSalary(int salary) {
        money_balance += salary;
        unemployment_counter = 0;

        // ТЗ п.3.2.4: Если зарплата превышает желаемую — с вероятностью 30% ожидания растут
        if (salary > desired_salary && dis_int(gen) < 30) {
            desired_salary += 1;
        }
        // ТЗ п.3.2.4: Если зарплата ниже желаемой — с вероятностью 20% ожидания снижаются
        else if (salary < desired_salary && dis_int(gen) < 20 && desired_salary > 3) {
            desired_salary -= 1;
        }
    }

    // ТЗ п.3.2.4: Адаптация безработных — снижение ожиданий
    // (30% + 10% за каждый ход безработицы, но не более 90%)
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

// ============================================================================
// ТЗ п.2: Производитель сырья — владеет заводом по производству сырья
// Параметры: название, баланс, запасы сырья, макс. рабочих, уровень производства
// Цель: максимизировать накопленную денежную массу
// ============================================================================
class RawProducer : public Entity {
public:
    int raw_storage;               // ТЗ п.2: текущие запасы сырья
    int raw_price;                 // ТЗ п.2: цена за сырьё
    int salary_rate;               // ТЗ п.2: заработная плата
    int max_workers;               // ТЗ п.2: максимальное количество рабочих
    int current_workers;           // Текущее количество нанятых рабочих
    int reserved_salary;           // ТЗ п.3.2.2: зарезервированный зарплатный фонд
    int last_sold;                 // Продано в предыдущем ходе
    int last_produced;             // Произведено в предыдущем ходе
    int production_cost;           // ТЗ п.3.2.4: себестоимость единицы продукции
    int total_salary_cost;         // Общие затраты на зарплату
    bool is_player_controlled;     // ТЗ п.4.8: флаг управления игроком
    int idle_turns;                // Счётчик ходов без производства

    RawProducer(int _id, const string& _name, int _money,
        int _raw_price, int _salary, int _max_workers,
        bool _is_player = false)
        : Entity(_id, _name, _money), raw_storage(10), raw_price(_raw_price),
        salary_rate(_salary), max_workers(_max_workers), current_workers(0),
        reserved_salary(0), last_sold(0), last_produced(0),
        production_cost(raw_price), total_salary_cost(0),
        is_player_controlled(_is_player), idle_turns(0) {
    }

    // ТЗ п.3.2.3: Доступные средства с учётом резерва
    int getAvailableMoney() const {
        return money_balance - reserved_salary;
    }

    // ========================================================================
    // ТЗ п.3.2.1: Фаза 1 — Установка цен и заработных плат
    // ТЗ п.3.4.1: Стратегия производителей сырья
    // ========================================================================
    void adjust() {
        // ТЗ п.4.8: Метод adjust() вызывается только для ИИ-агентов
        if (is_player_controlled) return;

        if (last_produced == 0) {
            idle_turns++;
        }
        else {
            idle_turns = 0;
        }

        if (idle_turns >= 3) {
            raw_price = max(1, decreasePrice(raw_price, 70));
            salary_rate = max(3, increasePrice(salary_rate, 120));

            if (money_balance > 100 && current_workers == 0) {
                salary_rate = increasePrice(salary_rate, 120);
            }
            return;
        }

        // ТЗ п.3.2.1 п.1: Оценка прибыльности предыдущего хода
        // Антикризисное управление при убытках
        if (!wasLastTurnProfitable()) {
            salary_rate = decreasePrice(salary_rate, 90);
            raw_price = max(production_cost, raw_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        // ТЗ п.3.2.1 п.2: Обработка отсутствия производства
        // Повышение зарплаты для найма рабочих и восстановления производства
        if (last_produced == 0) {
            raw_price = max(production_cost, decreasePrice(raw_price, 80));
            salary_rate = increasePrice(salary_rate, 120);
            raw_price = max(1, raw_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        // ТЗ п.3.2.1 п.3: Анализ соотношения продаж и производства
        int sales_ratio = last_sold * 100 / max(1, last_produced);

        // ТЗ п.3.2.1 п.3: Высокий спрос (≥90%) — цена +20%, зарплата +10%
        if (sales_ratio >= 90) {
            raw_price = increasePrice(raw_price, 120);
            salary_rate = increasePrice(salary_rate, 110);
        }
        // ТЗ п.3.2.1 п.3: Хороший спрос (≥70%) — цена +10%, зарплата +10%
        else if (sales_ratio >= 70) {
            raw_price = increasePrice(raw_price, 110);
            salary_rate = increasePrice(salary_rate, 110);
        }
        // ТЗ п.3.2.1 п.3: Средний спрос (≥20%) — цена -10%, зарплата -10%
        else if (sales_ratio >= 20) {
            raw_price = decreasePrice(raw_price, 90);
            salary_rate = decreasePrice(salary_rate, 90);
        }
        // ТЗ п.3.2.1 п.3: Низкий спрос (<20%) — цена -30%, зарплата -20%
        else {
            raw_price = decreasePrice(raw_price, 70);
            salary_rate = decreasePrice(salary_rate, 80);
        }

        // ТЗ п.3.2.1 п.4: Коррекция на основе складских запасов
        // Затоваривание (>50) — цена -10%; дефицит (<5) при высоком спросе — цена +10%
        if (raw_storage > 50) {
            raw_price = decreasePrice(raw_price, 90);
        }
        else if (raw_storage < 5 && sales_ratio > 80) {
            raw_price = increasePrice(raw_price, 110);
        }

        // ТЗ п.3.2.1 п.6: Проверка финансовой состоятельности
        // Если баланс недостаточен для полной загрузки — зарплата снижается
        int estimated_salary_cost = max_workers * salary_rate;
        if (money_balance < estimated_salary_cost && max_workers > 0) {
            salary_rate = max(3, money_balance / max_workers);
        }

        if (money_balance > 150 && current_workers == 0 && idle_turns > 0) {
            salary_rate = increasePrice(salary_rate, 130);
        }

        // ТЗ п.3.2.1 п.7: Применение ограничений
        // Цена не ниже себестоимости, зарплата не менее 3
        raw_price = max(production_cost, raw_price);
        raw_price = max(1, raw_price);
        salary_rate = max(3, salary_rate);
    }

    // ТЗ п.3.2.4 п.1: Производство сырья
    // 2 единицы сырья на рабочего за ход
    void produce() {
        total_salary_cost = reserved_salary;
        money_balance -= reserved_salary;
        reserved_salary = 0;

        int produced = current_workers * 2;
        raw_storage += produced;
        last_produced = produced;

        // ТЗ п.3.2.4: Себестоимость = суммарные затраты на зарплату / объём производства
        if (produced > 0) {
            production_cost = (total_salary_cost + produced - 1) / produced;
            if (production_cost < 1) production_cost = 1;
        }
    }

    // ТЗ п.3.2.3: Продажа сырья
    void sellRaw(int volume, int price) {
        volume = min(volume, raw_storage);
        if (volume > 0) {
            money_balance += volume * price;
            raw_storage -= volume;
        }
    }
};

// ============================================================================
// ТЗ п.2: Производитель товаров — владеет заводом по производству товаров
// Параметры: название, баланс, запасы сырья, запасы товаров,
// макс. рабочих, уровень производства
// Цель: максимизировать накопленную денежную массу
// ============================================================================
class GoodProducer : public Entity {
public:
    int raw_storage;               // ТЗ п.2: текущие запасы сырья
    int goods_storage;             // ТЗ п.2: текущие запасы готовой продукции
    int goods_price;               // ТЗ п.2: цена за товар
    int salary_rate;               // ТЗ п.2: заработная плата
    int max_workers;               // ТЗ п.2: максимальное количество рабочих
    int current_workers;           // Текущее количество рабочих
    int reserved_salary;           // ТЗ п.3.2.2: зарезервированный зарплатный фонд
    int last_sold;                 // Продано в предыдущем ходе
    int last_produced;             // Произведено в предыдущем ходе
    int production_cost;           // ТЗ п.3.2.4: себестоимость единицы товара
    int total_raw_cost;            // Общие затраты на сырьё
    int total_salary_cost;         // Общие затраты на зарплату
    bool is_player_controlled;     // ТЗ п.4.8: флаг управления игроком
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

    // ТЗ п.3.2.3: Доступные средства с учётом резерва
    int getAvailableMoney() const {
        return money_balance - reserved_salary;
    }

    // ========================================================================
    // ТЗ п.3.2.1: Фаза 1 — Установка цен и заработных плат
    // ТЗ п.3.4.2: Стратегия производителей товаров
    // ========================================================================
    void adjust() {
        // ТЗ п.4.8: Метод adjust() вызывается только для ИИ-агентов
        if (is_player_controlled) return;

        if (last_produced == 0) {
            idle_turns++;
        }
        else {
            idle_turns = 0;
        }

        if (idle_turns >= 3) {
            goods_price = max(1, decreasePrice(goods_price, 70));
            salary_rate = max(3, increasePrice(salary_rate, 120));

            if (money_balance > 100 && current_workers == 0) {
                salary_rate = increasePrice(salary_rate, 120);
            }
            return;
        }

        // ТЗ п.3.2.1 п.1: Оценка прибыльности предыдущего хода
        if (!wasLastTurnProfitable()) {
            salary_rate = decreasePrice(salary_rate, 90);
            goods_price = max(production_cost, goods_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        // ТЗ п.3.2.1 п.2: Обработка отсутствия производства
        if (last_produced == 0) {
            goods_price = max(production_cost, decreasePrice(goods_price, 80));
            salary_rate = increasePrice(salary_rate, 120);
            goods_price = max(1, goods_price);
            salary_rate = max(3, salary_rate);
            return;
        }

        // ТЗ п.3.2.1 п.3: Анализ соотношения продаж и производства
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

        // ТЗ п.3.2.1 п.4: Коррекция на основе складских запасов
        if (goods_storage > 50) {
            goods_price = decreasePrice(goods_price, 90);
        }
        else if (goods_storage < 5 && sales_ratio > 80) {
            goods_price = increasePrice(goods_price, 110);
        }

        // ТЗ п.3.2.1 п.5: Учёт обеспеченности сырьём
        // Дефицит сырья (<5) — цена +10%; избыток (>30) — цена -10%
        if (raw_storage < 5) {
            goods_price = increasePrice(goods_price, 110);
        }
        else if (raw_storage > 30) {
            goods_price = decreasePrice(goods_price, 90);
        }

        // ТЗ п.3.2.1 п.6: Проверка финансовой состоятельности
        int estimated_salary_cost = max_workers * salary_rate;
        if (money_balance < estimated_salary_cost && max_workers > 0) {
            salary_rate = max(3, money_balance / max_workers);
        }

        if (money_balance > 150 && current_workers == 0 && idle_turns > 0) {
            salary_rate = increasePrice(salary_rate, 130);
        }

        // ТЗ п.3.2.1 п.7: Применение ограничений
        goods_price = max(production_cost, goods_price);
        goods_price = max(1, goods_price);
        salary_rate = max(3, salary_rate);
    }

    // ТЗ п.3.2.4 п.2: Производство товаров
    // 1 единица сырья на 1 единицу товара, не более имеющегося запаса
    void produce() {
        total_salary_cost = reserved_salary;
        money_balance -= reserved_salary;
        reserved_salary = 0;

        int raw_needed = current_workers * 2;
        int raw_used = min(raw_needed, raw_storage);
        raw_storage -= raw_used;
        goods_storage += raw_used;
        last_produced = raw_used;

        // ТЗ п.3.2.4: Себестоимость = средневзвешенная стоимость сырья + удельные затраты на ЗП
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

    // ТЗ п.3.2.3: Закупка сырья
    void buyRaw(int volume, int price) {
        int cost = volume * price;
        if (getAvailableMoney() >= cost) {
            money_balance -= cost;
            raw_storage += volume;
            total_raw_cost += cost;
        }
    }

    // ТЗ п.3.2.3: Продажа товаров
    void sellGoods(int volume, int price) {
        volume = min(volume, goods_storage);
        if (volume > 0) {
            money_balance += volume * price;
            goods_storage -= volume;
        }
    }
};

// ============================================================================
// ТЗ п.3: Главный класс экономического симулятора
// ТЗ п.4: Реализация интерактивного режима
// ТЗ п.5: Реализация логгирования
// ============================================================================
class EconomicSimulator {
private:
    vector<Worker> workers;
    vector<RawProducer> raw_producers;
    vector<GoodProducer> good_producers;
    int current_turn;

    // ТЗ п.5.2: Файловая структура логов
    ofstream log_file;         // simulation_log.txt
    ofstream market_log;       // market_log.txt
    ofstream producer_log;     // producer_log.txt

    // ТЗ п.4.8: Параметры игрока
    int player_company_type;   // 0 - наблюдатель, 1 - сырьевая, 2 - товарная
    int player_company_id;     // ID компании игрока

public:
    // ТЗ п.5.5: Управление жизненным циклом файлов по принципу RAII
    EconomicSimulator() : current_turn(0), player_company_type(0), player_company_id(-1) {
        log_file.open("simulation_log.txt");
        market_log.open("market_log.txt");
        producer_log.open("producer_log.txt");
    }

    // ТЗ п.5.5: RAII — гарантированное закрытие файлов в деструкторе
    ~EconomicSimulator() {
        if (log_file.is_open()) log_file.close();
        if (market_log.is_open()) market_log.close();
        if (producer_log.is_open()) producer_log.close();
    }

    // ТЗ п.4.3.4: Вспомогательный метод для получения названия компании
    // для отображения информации о рынке труда
    string getCompanyName(int type, int id) {
        if (type == 1 && id >= 1 && id <= (int)raw_producers.size()) {
            return raw_producers[id - 1].name;
        }
        else if (type == 2 && id >= 1 && id <= (int)good_producers.size()) {
            return good_producers[id - 1].name;
        }
        return "Неизвестно";
    }

    // ========================================================================
    // ТЗ п.4.2: Выбор роли пользователя
    // ТЗ п.3.2.2 п.1: Создание агентов (инициализация)
    // ========================================================================
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

        // ТЗ п.2: Создание рабочих (12 агентов)
        for (int i = 0; i < 12; i++) {
            workers.emplace_back(i + 1, "Worker_" + to_string(i + 1));
        }

        // ТЗ п.4.2: Выбор роли — управление производителем сырья
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
        // ТЗ п.4.2: Выбор роли — управление производителем товаров
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
        // ТЗ п.4.2: Режим наблюдателя — все агенты управляются ИИ
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

        // ТЗ п.5.4.1: Логгирование фазы инициализации
        logToFile("=== СИМУЛЯЦИЯ НАЧАТА ===");
        logToFile("Создано рабочих: " + to_string(workers.size()));
        logToFile("Создано производителей сырья: " + to_string(raw_producers.size()));
        logToFile("Создано производителей товаров: " + to_string(good_producers.size()));

        cout << "\nНажмите Enter для продолжения...";
        cin.ignore();
        cin.get();
    }

    // ТЗ п.5.5: Метод записи в основной лог
    void logToFile(const string& msg) {
        if (log_file.is_open()) {
            log_file << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    // ТЗ п.5.5: Метод записи в лог рынка
    void logMarket(const string& msg) {
        if (market_log.is_open()) {
            market_log << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    // ТЗ п.5.5: Метод записи в лог производителей
    void logProducer(const string& msg) {
        if (producer_log.is_open()) {
            producer_log << "[Ход " << current_turn << "] " << msg << endl;
        }
    }

    // ТЗ п.3.2.1: Сохранение балансов для оценки прибыльности
    void saveAllBalances() {
        for (auto& w : workers) w.saveBalance();
        for (auto& r : raw_producers) r.saveBalance();
        for (auto& g : good_producers) g.saveBalance();
    }

    // ТЗ п.4.5: Интеграция интерактивного управления с игровым циклом
    void playerControlMenu() {
        if (player_company_type == 1) {
            controlRawProducer();
        }
        else if (player_company_type == 2) {
            controlGoodProducer();
        }
    }

    // ========================================================================
    // ТЗ п.4.3: Информационная панель игрока — управление сырьевой компанией
    // ТЗ п.4.3.1: Информация о собственной компании
    // ТЗ п.4.3.2: Информация о конкурентах
    // ТЗ п.4.3.3: Информация о контрагентах
    // ТЗ п.4.3.4: Информация о рынке труда
    // ТЗ п.4.4: Механика принятия решений
    // ========================================================================
    void controlRawProducer() {
        auto& player = raw_producers[player_company_id - 1];

        // ТЗ п.4.3.1: Информация о собственной компании
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

        // ТЗ п.4.3.2: Информация о конкурентах (производители сырья)
        cout << "\n--- Конкуренты (производители сырья) ---" << endl;
        for (auto& r : raw_producers) {
            if (!r.is_player_controlled) {
                cout << "  " << r.name << ": цена=" << r.raw_price
                    << ", запас=" << r.raw_storage
                    << ", рабочих=" << r.current_workers << "/" << r.max_workers << endl;
            }
        }

        // ТЗ п.4.3.3: Информация о контрагентах (покупатели — производители товаров)
        cout << "\n--- Покупатели (производители товаров) ---" << endl;
        for (auto& g : good_producers) {
            cout << "  " << g.name << ": баланс=" << g.money_balance
                << ", запас сырья=" << g.raw_storage << endl;
        }

        // ТЗ п.4.3.4: Информация о рынке труда
        cout << "\n--- Рынок труда ---" << endl;
        for (auto& w : workers) {
            cout << "  " << w.name << ": баланс=" << w.money_balance
                << ", желаемая ЗП=" << w.desired_salary;

            if (w.employer_id != -1) {
                string company = getCompanyName(w.employer_type, w.employer_id);
                cout << ", работает на: " << company;
            }
            else {
                cout << ", безработный";
            }
            cout << endl;
        }

        // ТЗ п.4.4: Меню принятия решений
        cout << "\n--- Действия ---" << endl;
        cout << "1. Изменить цену сырья" << endl;
        cout << "2. Изменить зарплату" << endl;
        cout << "3. Ничего не менять" << endl;
        cout << "Ваш выбор: ";

        int choice;
        cin >> choice;

        switch (choice) {
            // ТЗ п.4.4 п.1: Изменение цены продукции
            // ТЗ п.4.6: Защита от нулевых и отрицательных значений (мин. цена = 1)
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

            if (new_price < 1) new_price = 1;
            player.raw_price = new_price;
            break;
        }
              // ТЗ п.4.4 п.2: Изменение заработной платы
              // ТЗ п.4.6: Минимальное значение зарплаты — 3
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
            if (new_salary < 3) new_salary = 3;
            player.salary_rate = new_salary;
            break;
        }
              // ТЗ п.4.4 п.3: Сохранение текущих параметров
        case 3:
            cout << "Настройки сохранены без изменений." << endl;
            break;
        default:
            cout << "Неверный выбор. Настройки не изменены." << endl;
        }
    }

    // ========================================================================
    // ТЗ п.4.3: Информационная панель игрока — управление товарной компанией
    // ========================================================================
    void controlGoodProducer() {
        auto& player = good_producers[player_company_id - 1];

        // ТЗ п.4.3.1: Информация о собственной компании
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

        // ТЗ п.4.3.2: Информация о конкурентах (производители товаров)
        cout << "\n--- Конкуренты (производители товаров) ---" << endl;
        for (auto& g : good_producers) {
            if (!g.is_player_controlled) {
                cout << "  " << g.name << ": цена=" << g.goods_price
                    << ", запас=" << g.goods_storage
                    << ", рабочих=" << g.current_workers << "/" << g.max_workers << endl;
            }
        }

        // ТЗ п.4.3.3: Информация о контрагентах (поставщики — производители сырья)
        cout << "\n--- Поставщики (производители сырья) ---" << endl;
        for (auto& r : raw_producers) {
            cout << "  " << r.name << ": цена=" << r.raw_price
                << ", запас=" << r.raw_storage << endl;
        }

        // ТЗ п.4.3.4: Информация о рынке труда
        cout << "\n--- Рынок труда ---" << endl;
        for (auto& w : workers) {
            cout << "  " << w.name << ": баланс=" << w.money_balance
                << ", желаемая ЗП=" << w.desired_salary;

            if (w.employer_id != -1) {
                string company = getCompanyName(w.employer_type, w.employer_id);
                cout << ", работает на: " << company;
            }
            else {
                cout << ", безработный";
            }
            cout << endl;
        }

        // ТЗ п.4.4: Меню принятия решений
        cout << "\n--- Действия ---" << endl;
        cout << "1. Изменить цену товаров" << endl;
        cout << "2. Изменить зарплату" << endl;
        cout << "3. Ничего не менять" << endl;
        cout << "Ваш выбор: ";

        int choice;
        cin >> choice;

        switch (choice) {
            // ТЗ п.4.4 п.1: Изменение цены продукции
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

            if (new_price < 1) new_price = 1;
            player.goods_price = new_price;
            break;
        }
              // ТЗ п.4.4 п.2: Изменение заработной платы
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
            if (new_salary < 3) new_salary = 3;
            player.salary_rate = new_salary;
            break;
        }
              // ТЗ п.4.4 п.3: Сохранение текущих параметров
        case 3:
            cout << "Настройки сохранены без изменений." << endl;
            break;
        default:
            cout << "Неверный выбор. Настройки не изменены." << endl;
        }
    }

    // ========================================================================
    // ТЗ п.3.2.1: Фаза 1 — Установка цен и заработных плат
    // ========================================================================
    void phase1_SetPricesAndSalaries() {
        // ТЗ п.5.4.2: Логгирование заголовка фазы 1
        logToFile("\n=== ФАЗА 1: Установка цен и зарплат ===");

        // ТЗ п.4.5: Обработка хода игрока
        if (player_company_type > 0) {
            playerControlMenu();
        }

        // ТЗ п.4.8: Метод adjust() вызывается только для ИИ-агентов
        for (auto& r : raw_producers) {
            if (!r.is_player_controlled) {
                r.adjust();
            }
            // ТЗ п.5.4.2: Логгирование решений производителей сырья
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
            // ТЗ п.5.4.2: Логгирование решений производителей товаров
            logProducer(g.name + ": цена=" + to_string(g.goods_price) +
                ", зарплата=" + to_string(g.salary_rate) +
                ", баланс=" + to_string(g.money_balance) +
                ", продано/произведено=" + to_string(g.last_sold) +
                "/" + to_string(g.last_produced));
        }
    }

    // ========================================================================
    // ТЗ п.3.2.2: Фаза 2 — Рынок труда
    // ========================================================================
    void phase2_LaborMarket() {
        // ТЗ п.5.4.3: Логгирование заголовка фазы 2
        logToFile("\n=== ФАЗА 2: Рынок труда ===");

        // ТЗ п.3.2.2 п.1: Сброс состояния занятости
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

        // ТЗ п.3.2.2 п.2: Формирование заявок на наём от производителей сырья
        for (auto& r : raw_producers) {
            int available_money = r.money_balance;
            int affordable_workers = available_money / max(1, r.salary_rate);
            int workers_to_hire = min(r.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ r.salary_rate, 1, r.id });
            }
        }

        // ТЗ п.3.2.2 п.2: Формирование заявок на наём от производителей товаров
        for (auto& g : good_producers) {
            int available_money = g.money_balance;
            int affordable_workers = available_money / max(1, g.salary_rate);
            int workers_to_hire = min(g.max_workers, affordable_workers);

            for (int i = 0; i < workers_to_hire; i++) {
                offers.push_back({ g.salary_rate, 2, g.id });
            }
        }

        // ТЗ п.3.2.2 п.3: Случайное перемешивание заявок
        shuffle(offers.begin(), offers.end(), gen);

        // ТЗ п.3.2.2 п.4: Сортировка заявок по убыванию заработной платы
        sort(offers.begin(), offers.end(),
            [](const Offer& a, const Offer& b) { return a.salary > b.salary; });

        // ТЗ п.3.2.2 п.5: Случайное перемешивание очереди рабочих
        vector<int> worker_indices(workers.size());
        for (int i = 0; i < (int)workers.size(); i++) {
            worker_indices[i] = i;
        }
        shuffle(worker_indices.begin(), worker_indices.end(), gen);

        int hired_count = 0;

        // ТЗ п.3.2.2 п.6: Распределение рабочих с учётом желаемого уровня зарплаты
        for (auto& offer : offers) {
            for (int idx : worker_indices) {
                auto& w = workers[idx];
                if (w.employer_id != -1) continue;

                // ТЗ п.3.2.2 п.6: Условие найма: предлагаемая_зарплата >= желаемая_зарплата
                if (offer.salary >= w.desired_salary) {
                    if (offer.type == 1) {
                        auto& r = raw_producers[offer.id - 1];
                        if (r.current_workers < r.max_workers) {
                            int new_reserved = r.reserved_salary + r.salary_rate;
                            if (r.money_balance >= new_reserved) {
                                r.current_workers++;
                                // ТЗ п.3.2.2 п.7: Резервирование зарплатного фонда
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
                                // ТЗ п.3.2.2 п.7: Резервирование зарплатного фонда
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

        // ТЗ п.5.4.3: Логгирование результата найма
        logToFile("Нанято рабочих: " + to_string(hired_count) +
            " из " + to_string(workers.size()));
    }

    // ========================================================================
    // ТЗ п.3.2.3: Фаза 3 — Торговля
    // ========================================================================
    void phase3_Trade() {
        // ТЗ п.5.4.4: Логгирование заголовка фазы 3
        logToFile("\n=== ФАЗА 3: Торговля ===");

        for (auto& r : raw_producers) r.last_sold = 0;
        for (auto& g : good_producers) g.last_sold = 0;

        // ТЗ п.5.4.4: Разделитель секции рынка сырья (B2B)
        logMarket("--- Рынок сырья (B2B) ---");

        // ТЗ п.3.2.3: Случайное перемешивание очереди покупателей
        vector<int> buyer_indices(good_producers.size());
        for (int i = 0; i < (int)good_producers.size(); i++) {
            buyer_indices[i] = i;
        }
        shuffle(buyer_indices.begin(), buyer_indices.end(), gen);

        for (int idx : buyer_indices) {
            auto& g = good_producers[idx];
            // ТЗ п.3.2.3: Потребность в сырье = 10 - текущий запас
            int need = max(0, 10 - g.raw_storage);

            vector<pair<int, RawProducer*>> sorted_sellers;
            for (auto& r : raw_producers) {
                if (r.raw_storage > 0) {
                    sorted_sellers.push_back({ r.raw_price, &r });
                }
            }

            // ТЗ п.3.2.3: При равенстве цен порядок определяется случайным образом
            shuffle(sorted_sellers.begin(), sorted_sellers.end(), gen);
            // ТЗ п.3.2.3: Ранжирование продавцов по возрастанию цены
            sort(sorted_sellers.begin(), sorted_sellers.end());

            for (auto& [price, r] : sorted_sellers) {
                if (need <= 0) break;
                if (r->raw_price <= 0) continue;

                // ТЗ п.3.2.3: Проверка достаточности средств с учётом резерва
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

                    // ТЗ п.5.4.4: Логгирование сделки
                    logMarket(g.name + " купил " + to_string(buy) +
                        " сырья у " + r->name + " по цене " +
                        to_string(r->raw_price));
                }
            }
        }

        // ТЗ п.5.4.4: Разделитель секции рынка товаров (B2C)
        logMarket("--- Рынок товаров (B2C) ---");

        // ТЗ п.3.2.3: Случайное перемешивание очереди покупателей
        vector<int> worker_indices(workers.size());
        for (int i = 0; i < (int)workers.size(); i++) {
            worker_indices[i] = i;
        }
        shuffle(worker_indices.begin(), worker_indices.end(), gen);

        for (int idx : worker_indices) {
            auto& w = workers[idx];

            vector<pair<int, GoodProducer*>> sorted_sellers;
            for (auto& g : good_producers) {
                if (g.goods_storage > 0) {
                    sorted_sellers.push_back({ g.goods_price, &g });
                }
            }

            // ТЗ п.3.2.3: При равенстве цен порядок определяется случайным образом
            shuffle(sorted_sellers.begin(), sorted_sellers.end(), gen);
            // ТЗ п.3.2.3: Ранжирование продавцов по возрастанию цены
            sort(sorted_sellers.begin(), sorted_sellers.end());

            for (auto& [price, g] : sorted_sellers) {
                if (g->goods_price <= 0) continue;

                int max_affordable = w.money_balance / g->goods_price;
                int buy = min(g->goods_storage, max_affordable);

                if (buy <= 0) continue;

                w.buyGoods(g->goods_price, buy);
                g->sellGoods(buy, g->goods_price);
                g->last_sold += buy;

                // ТЗ п.5.4.4: Логгирование сделки
                logMarket(w.name + " купил " + to_string(buy) +
                    " товаров у " + g->name + " по цене " +
                    to_string(g->goods_price));
            }
        }
    }

    // ========================================================================
    // ТЗ п.3.2.4: Фаза 4 — Производство и выплата зарплат
    // ========================================================================
    void phase4_Production() {
        // ТЗ п.5.4.5: Логгирование заголовка фазы 4
        logToFile("\n=== ФАЗА 4: Производство и выплата зарплат ===");

        // ТЗ п.3.2.4 п.1: Производство сырья
        for (auto& r : raw_producers) {
            if (r.current_workers > 0) {
                r.produce();
                // ТЗ п.5.4.5: Логгирование результата производства сырья
                logToFile(r.name + " произвёл " + to_string(r.last_produced) +
                    " сырья, рабочих: " + to_string(r.current_workers) +
                    ", баланс: " + to_string(r.money_balance) +
                    ", запас: " + to_string(r.raw_storage));
            }
            else {
                r.last_produced = 0;
                // ТЗ п.5.4.5: Логгирование неактивного завода
                logToFile(r.name + " не производил - нет рабочих");
            }
        }

        // ТЗ п.3.2.4 п.2: Производство товаров
        for (auto& g : good_producers) {
            if (g.current_workers > 0) {
                g.produce();
                // ТЗ п.5.4.5: Логгирование результата производства товаров
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
        // ТЗ п.3.2.4 п.3: Выплата заработной платы рабочим
        for (auto& w : workers) {
            if (w.employer_type == 1) {
                w.earnSalary(raw_producers[w.employer_id - 1].salary_rate);
            }
            else if (w.employer_type == 2) {
                w.earnSalary(good_producers[w.employer_id - 1].salary_rate);
            }
            else {
                unemployed++;
                // ТЗ п.3.2.4 п.4: Адаптация безработных
                w.adaptUnemployed();
            }
        }

        // ТЗ п.5.4.5: Итоговая запись о количестве безработных
        logToFile("Безработных: " + to_string(unemployed));
    }

    // ========================================================================
    // ТЗ п.3.5: Статистический учёт и отображение результатов
    // ========================================================================
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

        // ТЗ п.3.5: Средний баланс, среднее кол-во товаров, уровень занятости,
        // средняя желаемая ЗП
        cout << "\n--- Рабочие ---" << endl;
        cout << "  Средний баланс: " << total_worker_money / (int)workers.size() << endl;
        cout << "  Среднее количество товаров: " << total_worker_goods / (int)workers.size() << endl;
        cout << "  Занятость: " << employed << "/" << workers.size() << endl;
        cout << "  Средняя желаемая ЗП: " << avg_desired_salary / (int)workers.size() << endl;

        // ТЗ п.3.5: Для каждого производителя — финансовое состояние,
        // складские запасы, уровень занятости, ценовая политика, результаты продаж
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

    // ========================================================================
    // ТЗ п.3.2: Игровой цикл автономного режима — 4 фазы
    // ========================================================================
    void runSimulation(int turns) {
        for (int t = 1; t <= turns; t++) {
            current_turn = t;
            cout << "\n+----------------------------------------+" << endl;
            cout << "|              ХОД " << t << "                     |" << endl;
            cout << "+----------------------------------------+" << endl;

            saveAllBalances();
            phase1_SetPricesAndSalaries();   // ТЗ п.3.2.1
            phase2_LaborMarket();            // ТЗ п.3.2.2
            phase3_Trade();                  // ТЗ п.3.2.3
            phase4_Production();             // ТЗ п.3.2.4
            printStatistics();               // ТЗ п.3.5
        }

        // ТЗ п.5.5: Логгирование завершения симуляции
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

    // ========================================================================
    // ТЗ п.4.7 п.2: Просмотр состояния всех агентов
    // ========================================================================
    void showAllAgents() {
        cout << "\n========================================" << endl;
        cout << "    СОСТОЯНИЕ ВСЕХ АГЕНТОВ" << endl;
        cout << "========================================" << endl;

        // ТЗ п.4.7 п.2: Полная информация о рабочих
        cout << "\n--- Рабочие ---" << endl;
        for (auto& w : workers) {
            cout << "ID:" << w.id << " " << w.name
                << " | Баланс:" << w.money_balance
                << " | Товары:" << w.goods_stored
                << " | Желаемая ЗП:" << w.desired_salary;
            if (w.employer_id != -1) {
                string company = getCompanyName(w.employer_type, w.employer_id);
                cout << " | Работает на: " << company;
            }
            else {
                cout << " | Безработный";
            }
            cout << endl;
        }

        // ТЗ п.4.7 п.2: Полная информация о производителях сырья
        cout << "\n--- Производители сырья ---" << endl;
        for (auto& r : raw_producers) {
            cout << "ID:" << r.id << " " << r.name
                << " | Баланс:" << r.money_balance
                << " | Сырьё:" << r.raw_storage
                << " | Цена:" << r.raw_price
                << " | Зарплата:" << r.salary_rate
                << " | Рабочих:" << r.current_workers << "/" << r.max_workers
                << " | Себест.:" << r.production_cost
                << " | Продано:" << r.last_sold << endl;
        }

        // ТЗ п.4.7 п.2: Полная информация о производителях товаров
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
                << " | Продано:" << g.last_sold << endl;
        }
        cout << "========================================" << endl;
    }

    // ========================================================================
    // ТЗ п.4.7: Дополнительные возможности взаимодействия — главное меню
    // ========================================================================
    void interactiveMode() {
        cout << "\n========================================" << endl;
        cout << "    ИНТЕРАКТИВНЫЙ РЕЖИМ" << endl;
        cout << "========================================" << endl;

        while (true) {
            // ТЗ п.4.7: Главное меню
            cout << "\n--- Главное меню ---" << endl;
            cout << "1. Запустить симуляцию на N ходов" << endl;
            cout << "2. Показать состояние всех агентов" << endl;
            cout << "3. Выйти из программы" << endl;
            cout << "Ваш выбор: ";

            int choice;
            cin >> choice;

            switch (choice) {
                // ТЗ п.4.7 п.1: Запуск симуляции на N ходов
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
                  // ТЗ п.4.7 п.2: Просмотр состояния всех агентов
            case 2:
                showAllAgents();
                break;
                // ТЗ п.4.7 п.3: Завершение работы
            case 3:
                cout << "\nЗавершение программы..." << endl;
                return;
            default:
                cout << "Неверный выбор. Попробуйте снова." << endl;
            }
        }
    }
};

// ============================================================================
// ТЗ п.4: Точка входа — главная функция
// ============================================================================
int main() {
    // ТЗ п.6: Русский язык в интерфейсе
    setlocale(LC_ALL, "Russian");

    cout << "+------------------------------------------------------+" << endl;
    cout << "|        ЭКОНОМИЧЕСКИЙ СИМУЛЯТОР                    |" << endl;
    cout << "|        Версия: 1.6                                |" << endl;
    cout << "+------------------------------------------------------+" << endl;
    cout << endl;
    cout << "Особенности симулятора:" << endl;
    cout << "  * Замкнутая экономическая система" << endl;
    cout << "  * Три типа агентов: рабочие, производители сырья и товаров" << endl;
    cout << "  * Двухсекторная модель производства" << endl;
    cout << "  * Динамическое ценообразование на основе спроса и предложения" << endl;
    cout << "  * Рынок труда с учётом желаемых зарплат рабочих" << endl;
    cout << "  * Случайное перемешивание очередей для равных возможностей" << endl;
    cout << "  * Адаптивные ИИ-стратегии конкурентов" << endl;
    cout << "  * Интерактивное управление компанией" << endl;
    cout << "  * Детальное логгирование всех событий" << endl;

    EconomicSimulator simulator;
    simulator.initialize();      // ТЗ п.4.2: Выбор роли пользователя
    simulator.interactiveMode(); // ТЗ п.4.7: Главное меню

    cout << "\nСпасибо за использование экономического симулятора!" << endl;
    return 0;
}
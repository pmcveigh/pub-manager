#pragma once

#include <SDL2/SDL.h>

#include <deque>
#include <string>
#include <vector>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct InputState {
    bool upPressed = false;
    bool downPressed = false;
    bool enterPressed = false;
    bool toggleOverlayPressed = false;
    bool restartPressed = false;
};

enum class CustomerState {
    Entering,
    GoingToQueue,
    Queueing,
    BeingServed,
    GoingToSeatOrStanding,
    Drinking,
    GoingToSeat,
    Socializing,
    GoingToToilet,
    AtToilet,
    Leaving,
    Exited
};

struct Customer {
    int id = 0;
    Vec2 pos{};
    Vec2 target{};
    CustomerState state = CustomerState::Entering;
    float mood = 70.0f;
    float stateTimer = 0.0f;
    float waitTimer = 0.0f;
    float socialTimer = 0.0f;
    float nextMessTimer = 0.0f;
    int queueIndex = -1;
    int seatIndex = -1;
    bool wantsToilet = false;
    bool hasBeenServed = false;
    bool shouldRemove = false;
    int drinksConsumed = 0;
};

struct Mess {
    int id = 0;
    Vec2 pos{};
    float amount = 1.0f;
};

struct TableSpot {
    Vec2 pos{};
    bool occupied = false;
};

struct Staff {
    Vec2 pos{};
    Vec2 target{};
    float speed = 80.0f;
    float workTimer = 0.0f;
    int targetMessId = -1;
};

struct IncidentInfo {
    std::string text;
    float timer = 0.0f;
};

struct DayStats {
    float revenue = 0.0f;
    int drinkSales = 0;
    int incidents = 0;
    int customersServed = 0;
    int customersDeparted = 0;
    int customersUnhappy = 0;
    float totalFinalMood = 0.0f;
};

struct Layout {
    Rect floor{};
    Rect entrance{};
    Rect barCounter{};
    Rect queueArea{};
    Rect standingArea{};
    Rect toiletArea{};
    Rect exitArea{};
    std::vector<TableSpot> seats;
    std::vector<Vec2> queueSpots;
};

enum class SessionState {
    PreOpen,
    Running,
    Summary
};

class Game {
public:
    Game();
    void Reset();
    void Update(float dt, const InputState& input);
    void Draw(SDL_Renderer* renderer);

private:
    Layout layout_{};
    SessionState sessionState_ = SessionState::PreOpen;
    std::vector<Customer> customers_{};
    std::vector<Mess> messes_{};
    std::deque<int> barQueue_{};
    Staff bartender_{};
    Staff cleaner_{};
    DayStats stats_{};
    IncidentInfo incidentBanner_{};

    int nextCustomerId_ = 1;
    int nextMessId_ = 1;
    float timeRemaining_ = 0.0f;
    float spawnTimer_ = 0.0f;
    float incidentTimer_ = 0.0f;
    float toiletBlockedTimer_ = 0.0f;
    bool showOverlay_ = false;

    float drinkPrice_ = 5.0f;
    float stockBeer_ = 0.0f;
    float stockCider_ = 0.0f;
    int bartenderServingCustomerId_ = -1;
    float bartenderServeTimer_ = 0.0f;

    void BuildLayout();
    void HandleInput(const InputState& input);
    void UpdateRunning(float dt);
    void SpawnCustomer();
    void UpdateCustomer(Customer& c, float dt);
    void UpdateQueueTargets();
    void UpdateBartender(float dt);
    void UpdateCleaner(float dt);
    void UpdateIncidents(float dt);
    void AddMess(Vec2 pos, float amount);
    void ResolveDepartures();
    void EndDay();

    Customer* FindCustomer(int id);
    Mess* FindMess(int id);
    int FindFreeSeatIndex();
    void FreeSeat(int index);
    Vec2 RandomPointInRect(Rect r);
    bool MoveTowards(Vec2& pos, const Vec2& target, float speed, float dt, float arriveDistance = 4.0f);
    float AverageSatisfaction() const;
    float NetProfit() const;
};

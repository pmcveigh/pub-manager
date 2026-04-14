#include "entities.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>

Game::Game() {
    BuildLayout();
    Reset();
}

void Game::BuildLayout() {
    layout_.floor = {120, 80, 1040, 580};
    layout_.entrance = {140, 610, 80, 40};
    layout_.barCounter = {690, 110, 400, 80};
    layout_.queueArea = {670, 200, 320, 220};
    layout_.standingArea = {260, 180, 230, 210};
    layout_.toiletArea = {940, 470, 170, 130};
    layout_.exitArea = {1040, 610, 80, 40};

    layout_.seats = {
        {{340, 470}, false}, {{410, 470}, false}, {{480, 470}, false},
        {{340, 540}, false}, {{410, 540}, false}, {{480, 540}, false},
        {{600, 500}, false}, {{660, 500}, false}
    };

    layout_.queueSpots.clear();
    for (int i = 0; i < 6; ++i) {
        layout_.queueSpots.push_back({840.0f, 230.0f + i * 30.0f});
    }
}

void Game::Reset() {
    customers_.clear();
    messes_.clear();
    barQueue_.clear();
    stats_ = DayStats{};

    for (auto& s : layout_.seats) {
        s.occupied = false;
    }

    sessionState_ = SessionState::PreOpen;
    nextCustomerId_ = 1;
    nextMessId_ = 1;
    timeRemaining_ = Config::DayDuration;
    spawnTimer_ = 0.5f;
    incidentTimer_ = GetRandomValue((int)(Config::IncidentIntervalMin * 100), (int)(Config::IncidentIntervalMax * 100)) / 100.0f;
    toiletBlockedTimer_ = 0.0f;
    showOverlay_ = false;

    drinkPrice_ = Config::InitialDrinkPrice;
    stockBeer_ = Config::InitialStockBeer;
    stockCider_ = Config::InitialStockCider;

    bartender_.pos = {860, 150};
    bartender_.target = bartender_.pos;
    bartender_.speed = 0.0f;
    bartenderServingCustomerId_ = -1;
    bartenderServeTimer_ = 0.0f;

    cleaner_.pos = {230, 220};
    cleaner_.target = cleaner_.pos;
    cleaner_.speed = Config::CleanerMoveSpeed;
    cleaner_.workTimer = 0.0f;
    cleaner_.targetMessId = -1;

    incidentBanner_.text.clear();
    incidentBanner_.timer = 0.0f;
}

void Game::HandleInput() {
    if (sessionState_ == SessionState::PreOpen) {
        if (IsKeyPressed(KEY_UP)) drinkPrice_ += 0.5f;
        if (IsKeyPressed(KEY_DOWN)) drinkPrice_ -= 0.5f;
        drinkPrice_ = std::clamp(drinkPrice_, 2.0f, 14.0f);
        if (IsKeyPressed(KEY_ENTER)) {
            sessionState_ = SessionState::Running;
        }
    } else if (sessionState_ == SessionState::Running) {
        if (IsKeyPressed(KEY_M)) showOverlay_ = !showOverlay_;
    } else if (sessionState_ == SessionState::Summary) {
        if (IsKeyPressed(KEY_R)) Reset();
    }
}

void Game::Update(float dt) {
    HandleInput();

    if (incidentBanner_.timer > 0.0f) {
        incidentBanner_.timer -= dt;
    }

    if (sessionState_ == SessionState::Running) {
        UpdateRunning(dt);
    }
}

void Game::UpdateRunning(float dt) {
    timeRemaining_ -= dt;
    if (timeRemaining_ <= 0.0f) {
        EndDay();
        return;
    }

    if (toiletBlockedTimer_ > 0.0f) {
        toiletBlockedTimer_ -= dt;
    }

    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0.0f && (int)customers_.size() < Config::MaxCustomers) {
        SpawnCustomer();
        spawnTimer_ = Config::CustomerSpawnInterval + GetRandomValue(0, 120) / 100.0f;
    }

    UpdateQueueTargets();

    for (auto& c : customers_) {
        UpdateCustomer(c, dt);
    }

    UpdateBartender(dt);
    UpdateCleaner(dt);
    UpdateIncidents(dt);
    ResolveDepartures();
}

void Game::SpawnCustomer() {
    Customer c;
    c.id = nextCustomerId_++;
    c.pos = RandomPointInRect(layout_.entrance);
    c.target = {760, 330};
    c.state = CustomerState::Entering;
    c.stateTimer = 1.0f;
    c.socialTimer = 15.0f + GetRandomValue(0, 1200) / 100.0f;
    c.wantsToilet = GetRandomValue(0, 100) < 45;
    customers_.push_back(c);
}

void Game::UpdateCustomer(Customer& c, float dt) {
    for (const auto& m : messes_) {
        float dx = c.pos.x - m.pos.x;
        float dy = c.pos.y - m.pos.y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < 6000.0f) {
            c.mood -= Config::MoodDirtyPenaltyPerSecond * dt * m.amount;
        }
    }

    c.mood = std::clamp(c.mood, 0.0f, 100.0f);

    switch (c.state) {
        case CustomerState::Entering: {
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::GoingToQueue;
                barQueue_.push_back(c.id);
                c.queueIndex = (int)barQueue_.size() - 1;
            }
            break;
        }
        case CustomerState::GoingToQueue:
        case CustomerState::Queueing: {
            if (c.queueIndex < 0) {
                c.state = CustomerState::Queueing;
                break;
            }
            Vector2 queueTarget = layout_.queueSpots[std::min(c.queueIndex, (int)layout_.queueSpots.size() - 1)];
            MoveTowards(c.pos, queueTarget, Config::CustomerMoveSpeed, dt);
            c.state = CustomerState::Queueing;
            c.waitTimer += dt;
            c.mood -= Config::MoodWaitPenaltyPerSecond * dt;
            break;
        }
        case CustomerState::GoingToSeat: {
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::Socializing;
                c.stateTimer = 8.0f + GetRandomValue(0, 900) / 100.0f;
            }
            break;
        }
        case CustomerState::Socializing: {
            c.stateTimer -= dt;
            if (GetRandomValue(0, 1000) < 2) {
                AddMess({c.pos.x + GetRandomValue(-12, 12), c.pos.y + GetRandomValue(-12, 12)}, 0.7f);
            }
            if (c.stateTimer <= 0.0f) {
                if (c.wantsToilet && GetRandomValue(0, 100) < 60) {
                    c.state = CustomerState::GoingToToilet;
                    c.target = RandomPointInRect(layout_.toiletArea);
                    c.wantsToilet = false;
                } else if (c.drinksConsumed < 2 && GetRandomValue(0, 100) < 48) {
                    c.state = CustomerState::GoingToQueue;
                    barQueue_.push_back(c.id);
                    c.queueIndex = (int)barQueue_.size() - 1;
                } else {
                    c.state = CustomerState::Leaving;
                    c.target = RandomPointInRect(layout_.exitArea);
                }
            }
            break;
        }
        case CustomerState::GoingToToilet: {
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::AtToilet;
                c.stateTimer = 4.0f + GetRandomValue(0, 300) / 100.0f;
            }
            if (toiletBlockedTimer_ > 0.0f) {
                c.mood -= 1.8f * dt;
            }
            break;
        }
        case CustomerState::AtToilet: {
            c.stateTimer -= dt;
            if (toiletBlockedTimer_ > 0.0f) {
                c.mood -= 2.8f * dt;
            }
            if (c.stateTimer <= 0.0f) {
                if (c.drinksConsumed < 2 && GetRandomValue(0, 100) < 35) {
                    c.state = CustomerState::GoingToQueue;
                    barQueue_.push_back(c.id);
                    c.queueIndex = (int)barQueue_.size() - 1;
                } else {
                    c.state = CustomerState::Leaving;
                    c.target = RandomPointInRect(layout_.exitArea);
                }
            }
            break;
        }
        case CustomerState::Leaving: {
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt, 8.0f)) {
                c.shouldRemove = true;
            }
            break;
        }
    }

    if (c.mood <= 8.0f && c.state != CustomerState::Leaving) {
        c.state = CustomerState::Leaving;
        c.target = RandomPointInRect(layout_.exitArea);
    }
}

void Game::UpdateQueueTargets() {
    for (int i = 0; i < (int)barQueue_.size(); ++i) {
        auto* c = FindCustomer(barQueue_[i]);
        if (!c) continue;
        c->queueIndex = i;
    }
}

void Game::UpdateBartender(float dt) {
    if (bartenderServingCustomerId_ < 0) {
        if (!barQueue_.empty()) {
            bartenderServingCustomerId_ = barQueue_.front();
            bartenderServeTimer_ = Config::BartenderServiceBaseTime;
            bartenderServeTimer_ /= 1.0f;
        }
        return;
    }

    auto* c = FindCustomer(bartenderServingCustomerId_);
    if (!c) {
        bartenderServingCustomerId_ = -1;
        if (!barQueue_.empty()) barQueue_.pop_front();
        return;
    }

    if (c->queueIndex != 0) {
        bartenderServingCustomerId_ = -1;
        return;
    }

    Vector2 serveSpot = {layout_.barCounter.x + 120, layout_.barCounter.y + layout_.barCounter.height + 16};
    if (!MoveTowards(c->pos, serveSpot, Config::CustomerMoveSpeed, dt)) {
        return;
    }

    bartenderServeTimer_ -= dt;
    if (bartenderServeTimer_ > 0.0f) {
        return;
    }

    bool hasStock = (stockBeer_ + stockCider_) > 0.0f;
    if (!hasStock) {
        c->mood -= Config::MoodOutOfStockPenalty;
    } else {
        if (stockBeer_ >= stockCider_ && stockBeer_ > 0.0f) stockBeer_ -= 1.0f;
        else if (stockCider_ > 0.0f) stockCider_ -= 1.0f;

        stats_.revenue += drinkPrice_;
        stats_.drinkSales += 1;
        c->drinksConsumed += 1;
        c->mood -= std::max(0.0f, drinkPrice_ - Config::PriceBaseline) * Config::MoodHighPricePenalty;
        c->mood += 4.0f;
        if (!c->hasBeenServed) {
            c->hasBeenServed = true;
            stats_.customersServed += 1;
        }
    }

    if (!barQueue_.empty()) barQueue_.pop_front();
    c->queueIndex = -1;

    int seatIdx = FindFreeSeatIndex();
    if (seatIdx >= 0) {
        layout_.seats[seatIdx].occupied = true;
        c->seatIndex = seatIdx;
        c->target = layout_.seats[seatIdx].pos;
        c->state = CustomerState::GoingToSeat;
    } else {
        c->seatIndex = -1;
        c->target = RandomPointInRect(layout_.standingArea);
        c->state = CustomerState::GoingToSeat;
        c->mood -= Config::MoodNoSeatPenalty;
    }

    bartenderServingCustomerId_ = -1;
}

void Game::UpdateCleaner(float dt) {
    if (cleaner_.targetMessId < 0) {
        float best = 1e9f;
        int bestId = -1;
        for (const auto& m : messes_) {
            float dx = cleaner_.pos.x - m.pos.x;
            float dy = cleaner_.pos.y - m.pos.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                bestId = m.id;
            }
        }
        cleaner_.targetMessId = bestId;
        cleaner_.workTimer = Config::CleanerBaseCleanTime;
        if (bestId < 0) return;
    }

    auto* m = FindMess(cleaner_.targetMessId);
    if (!m) {
        cleaner_.targetMessId = -1;
        cleaner_.workTimer = 0.0f;
        return;
    }

    if (!MoveTowards(cleaner_.pos, m->pos, cleaner_.speed, dt, 8.0f)) {
        return;
    }

    cleaner_.workTimer -= dt;
    if (cleaner_.workTimer <= 0.0f) {
        m->amount -= 0.7f;
        cleaner_.workTimer = Config::CleanerBaseCleanTime;
        if (m->amount <= 0.0f) {
            int id = m->id;
            messes_.erase(std::remove_if(messes_.begin(), messes_.end(), [id](const Mess& item) { return item.id == id; }), messes_.end());
            cleaner_.targetMessId = -1;
        }
    }
}

void Game::UpdateIncidents(float dt) {
    incidentTimer_ -= dt;
    if (incidentTimer_ > 0.0f) return;

    incidentTimer_ = GetRandomValue((int)(Config::IncidentIntervalMin * 100), (int)(Config::IncidentIntervalMax * 100)) / 100.0f;
    stats_.incidents += 1;

    int type = GetRandomValue(0, 3);
    if (type == 0) {
        Vector2 p = RandomPointInRect(layout_.standingArea);
        AddMess(p, 1.4f);
        incidentBanner_.text = "Incident: Big spill in standing area";
        incidentBanner_.timer = 4.0f;
    } else if (type == 1) {
        toiletBlockedTimer_ = Config::ToiletBlockDuration;
        incidentBanner_.text = "Incident: Toilet blocked";
        incidentBanner_.timer = 4.0f;
    } else if (type == 2) {
        incidentBanner_.text = "Incident: Argument broke out";
        incidentBanner_.timer = 4.0f;
        Vector2 p = RandomPointInRect(layout_.standingArea);
        for (auto& c : customers_) {
            float dx = c.pos.x - p.x;
            float dy = c.pos.y - p.y;
            if (dx * dx + dy * dy < 12000.0f) {
                c.mood -= 12.0f;
                if (GetRandomValue(0, 100) < 18) {
                    c.state = CustomerState::Leaving;
                    c.target = RandomPointInRect(layout_.exitArea);
                }
            }
        }
    } else {
        float reduceBeer = std::min(stockBeer_, 7.0f);
        float reduceCider = std::min(stockCider_, 5.0f);
        stockBeer_ -= reduceBeer;
        stockCider_ -= reduceCider;
        incidentBanner_.text = "Incident: Supplier issue reduced stock";
        incidentBanner_.timer = 4.0f;
    }
}

void Game::AddMess(Vector2 pos, float amount) {
    Mess m;
    m.id = nextMessId_++;
    m.pos = pos;
    m.amount = amount;
    messes_.push_back(m);
}

void Game::ResolveDepartures() {
    for (auto& c : customers_) {
        if (!c.shouldRemove) continue;
        if (c.seatIndex >= 0) FreeSeat(c.seatIndex);
        stats_.customersDeparted += 1;
        stats_.totalFinalMood += c.mood;
        if (c.mood < 40.0f) stats_.customersUnhappy += 1;
        barQueue_.erase(std::remove(barQueue_.begin(), barQueue_.end(), c.id), barQueue_.end());
    }

    customers_.erase(std::remove_if(customers_.begin(), customers_.end(), [](const Customer& c) { return c.shouldRemove; }), customers_.end());
}

void Game::EndDay() {
    for (auto& c : customers_) {
        if (c.seatIndex >= 0) FreeSeat(c.seatIndex);
        stats_.customersDeparted += 1;
        stats_.totalFinalMood += c.mood;
        if (c.mood < 40.0f) stats_.customersUnhappy += 1;
    }
    customers_.clear();
    barQueue_.clear();
    sessionState_ = SessionState::Summary;
}

Customer* Game::FindCustomer(int id) {
    for (auto& c : customers_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

Mess* Game::FindMess(int id) {
    for (auto& m : messes_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

int Game::FindFreeSeatIndex() {
    for (int i = 0; i < (int)layout_.seats.size(); ++i) {
        if (!layout_.seats[i].occupied) return i;
    }
    return -1;
}

void Game::FreeSeat(int index) {
    if (index >= 0 && index < (int)layout_.seats.size()) {
        layout_.seats[index].occupied = false;
    }
}

Vector2 Game::RandomPointInRect(Rectangle r) {
    float x = r.x + GetRandomValue(6, (int)r.width - 6);
    float y = r.y + GetRandomValue(6, (int)r.height - 6);
    return {x, y};
}

bool Game::MoveTowards(Vector2& pos, const Vector2& target, float speed, float dt, float arriveDistance) {
    float dx = target.x - pos.x;
    float dy = target.y - pos.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= arriveDistance) return true;
    float step = speed * dt;
    if (step >= dist) {
        pos = target;
        return true;
    }
    pos.x += dx / dist * step;
    pos.y += dy / dist * step;
    return false;
}

float Game::AverageSatisfaction() const {
    if (stats_.customersDeparted == 0) return 0.0f;
    return stats_.totalFinalMood / stats_.customersDeparted;
}

float Game::NetProfit() const {
    return stats_.revenue - Config::BaseWageBartender - Config::BaseWageCleaner;
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(Color{26, 28, 34, 255});

    DrawRectangleRec(layout_.floor, Color{74, 63, 53, 255});
    DrawRectangleLinesEx(layout_.floor, 3, Color{190, 170, 140, 255});

    DrawRectangleRec(layout_.entrance, Color{80, 130, 210, 255});
    DrawRectangleRec(layout_.exitArea, Color{70, 210, 130, 255});
    DrawRectangleRec(layout_.barCounter, Color{120, 78, 42, 255});
    DrawRectangleRec(layout_.standingArea, Color{92, 88, 84, 255});
    DrawRectangleRec(layout_.toiletArea, toiletBlockedTimer_ > 0.0f ? Color{180, 70, 70, 255} : Color{110, 130, 210, 255});
    DrawRectangleLinesEx(layout_.queueArea, 2, Color{230, 230, 230, 120});

    for (const auto& s : layout_.seats) {
        DrawCircleV(s.pos, 15, s.occupied ? Color{210, 150, 70, 255} : Color{155, 115, 70, 255});
    }

    for (const auto& q : layout_.queueSpots) {
        DrawCircleV(q, 4, Color{245, 245, 245, 110});
    }

    for (const auto& m : messes_) {
        float r = 6 + m.amount * 8;
        DrawCircleV(m.pos, r, Color{140, 220, 120, 220});
    }

    for (const auto& c : customers_) {
        Color base = Color{230, 210, 100, 255};
        if (c.mood < 40) base = Color{230, 120, 100, 255};
        if (c.mood > 75) base = Color{100, 220, 120, 255};
        DrawCircleV(c.pos, 10, base);
        if (showOverlay_) {
            DrawRectangle((int)c.pos.x - 14, (int)c.pos.y - 20, 28, 4, Color{40, 40, 40, 220});
            DrawRectangle((int)c.pos.x - 14, (int)c.pos.y - 20, (int)(28 * (c.mood / 100.0f)), 4, Color{90, 230, 110, 240});
        }
    }

    DrawCircleV(bartender_.pos, 12, Color{230, 120, 220, 255});
    DrawCircleV(cleaner_.pos, 12, Color{120, 220, 230, 255});

    DrawRectangle(0, 0, Config::ScreenWidth, 70, Color{15, 15, 18, 220});
    DrawText(TextFormat("Revenue: $%.0f", stats_.revenue), 20, 12, 22, RAYWHITE);
    DrawText(TextFormat("Stock B/C: %.0f / %.0f", stockBeer_, stockCider_), 230, 12, 22, RAYWHITE);
    DrawText(TextFormat("Queue: %i", (int)barQueue_.size()), 500, 12, 22, RAYWHITE);
    DrawText(TextFormat("Time: %.0f", std::max(0.0f, timeRemaining_)), 640, 12, 22, RAYWHITE);
    DrawText(TextFormat("Price: $%.1f", drinkPrice_), 760, 12, 22, RAYWHITE);
    DrawText(TextFormat("Mess: %i", (int)messes_.size()), 900, 12, 22, RAYWHITE);
    DrawText("Toggle overlay: M", 1020, 12, 20, Color{180, 180, 180, 255});

    DrawText("Entrance", (int)layout_.entrance.x, (int)layout_.entrance.y - 20, 18, RAYWHITE);
    DrawText("Exit", (int)layout_.exitArea.x + 15, (int)layout_.exitArea.y - 20, 18, RAYWHITE);
    DrawText("Bar", (int)layout_.barCounter.x + 160, (int)layout_.barCounter.y + 28, 24, RAYWHITE);
    DrawText("Standing", (int)layout_.standingArea.x + 55, (int)layout_.standingArea.y + 80, 20, RAYWHITE);
    DrawText("Toilets", (int)layout_.toiletArea.x + 45, (int)layout_.toiletArea.y + 45, 20, RAYWHITE);

    if (sessionState_ == SessionState::PreOpen) {
        DrawRectangle(220, 180, 840, 330, Color{10, 10, 16, 235});
        DrawText("PUB MANAGER PROTOTYPE", 430, 220, 40, RAYWHITE);
        DrawText("Adjust drink price with Up/Down", 430, 290, 28, RAYWHITE);
        DrawText(TextFormat("Current price: $%.1f", drinkPrice_), 430, 330, 32, Color{240, 220, 100, 255});
        DrawText("Press Enter to start day", 430, 390, 28, Color{130, 230, 180, 255});
    }

    if (incidentBanner_.timer > 0.0f && !incidentBanner_.text.empty()) {
        DrawRectangle(330, 78, 620, 38, Color{160, 56, 56, 230});
        DrawText(incidentBanner_.text.c_str(), 350, 87, 24, RAYWHITE);
    }

    if (sessionState_ == SessionState::Summary) {
        DrawRectangle(190, 120, 900, 470, Color{7, 7, 10, 240});
        DrawText("END OF DAY SUMMARY", 450, 160, 42, RAYWHITE);
        DrawText(TextFormat("Total revenue: $%.0f", stats_.revenue), 330, 240, 30, RAYWHITE);
        DrawText(TextFormat("Drink sales: %i", stats_.drinkSales), 330, 280, 30, RAYWHITE);
        DrawText(TextFormat("Staff wages: $%.0f", Config::BaseWageBartender + Config::BaseWageCleaner), 330, 320, 30, RAYWHITE);
        DrawText(TextFormat("Net profit: $%.0f", NetProfit()), 330, 360, 30, Color{130, 230, 180, 255});
        DrawText(TextFormat("Avg satisfaction: %.1f", AverageSatisfaction()), 330, 400, 30, RAYWHITE);
        DrawText(TextFormat("Incidents: %i", stats_.incidents), 330, 440, 30, RAYWHITE);
        DrawText(TextFormat("Customers served: %i", stats_.customersServed), 330, 480, 30, RAYWHITE);
        DrawText(TextFormat("Left unhappy: %i", stats_.customersUnhappy), 330, 520, 30, RAYWHITE);
        DrawText("Press R to restart", 510, 560, 30, Color{240, 220, 110, 255});
    }

    EndDrawing();
}

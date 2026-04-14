#include "entities.hpp"
#include "config.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace {
std::mt19937& Rng() {
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

int RandomInt(int minValue, int maxValue) {
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(Rng());
}

SDL_FRect ToSdlRect(Rect r) {
    return SDL_FRect{r.x, r.y, r.width, r.height};
}

void SetColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void FillRect(SDL_Renderer* renderer, Rect r) {
    SDL_FRect fr = ToSdlRect(r);
    SDL_RenderFillRectF(renderer, &fr);
}

void DrawRectOutline(SDL_Renderer* renderer, Rect r) {
    SDL_FRect fr = ToSdlRect(r);
    SDL_RenderDrawRectF(renderer, &fr);
}

void FillCenteredRect(SDL_Renderer* renderer, Vec2 center, float w, float h) {
    SDL_FRect rect{center.x - w * 0.5f, center.y - h * 0.5f, w, h};
    SDL_RenderFillRectF(renderer, &rect);
}

void DrawBar(SDL_Renderer* renderer, float x, float y, float width, float height, float normalized, Uint8 r, Uint8 g, Uint8 b) {
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    SetColor(renderer, 38, 38, 44, 240);
    SDL_FRect outer{x, y, width, height};
    SDL_RenderFillRectF(renderer, &outer);
    SetColor(renderer, r, g, b, 240);
    SDL_FRect inner{x + 2.0f, y + 2.0f, (width - 4.0f) * normalized, height - 4.0f};
    SDL_RenderFillRectF(renderer, &inner);
}
}

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
    incidentTimer_ = RandomInt(static_cast<int>(Config::IncidentIntervalMin * 100), static_cast<int>(Config::IncidentIntervalMax * 100)) / 100.0f;
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

void Game::HandleInput(const InputState& input) {
    if (sessionState_ == SessionState::PreOpen) {
        if (input.upPressed) drinkPrice_ += 0.5f;
        if (input.downPressed) drinkPrice_ -= 0.5f;
        drinkPrice_ = std::clamp(drinkPrice_, 2.0f, 14.0f);
        if (input.enterPressed) {
            sessionState_ = SessionState::Running;
        }
    } else if (sessionState_ == SessionState::Running) {
        if (input.toggleOverlayPressed) showOverlay_ = !showOverlay_;
    } else if (sessionState_ == SessionState::Summary) {
        if (input.restartPressed) Reset();
    }
}

void Game::Update(float dt, const InputState& input) {
    HandleInput(input);

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
    if (spawnTimer_ <= 0.0f && static_cast<int>(customers_.size()) < Config::MaxCustomers) {
        SpawnCustomer();
        spawnTimer_ = Config::CustomerSpawnInterval + RandomInt(0, 120) / 100.0f;
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
    c.socialTimer = 15.0f + RandomInt(0, 1200) / 100.0f;
    c.wantsToilet = RandomInt(0, 100) < 45;
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
        case CustomerState::Entering:
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::GoingToQueue;
                barQueue_.push_back(c.id);
                c.queueIndex = static_cast<int>(barQueue_.size()) - 1;
            }
            break;
        case CustomerState::GoingToQueue:
        case CustomerState::Queueing: {
            if (c.queueIndex < 0) {
                c.state = CustomerState::Queueing;
                break;
            }
            Vec2 queueTarget = layout_.queueSpots[std::min(c.queueIndex, static_cast<int>(layout_.queueSpots.size()) - 1)];
            MoveTowards(c.pos, queueTarget, Config::CustomerMoveSpeed, dt);
            c.state = CustomerState::Queueing;
            c.waitTimer += dt;
            c.mood -= Config::MoodWaitPenaltyPerSecond * dt;
            break;
        }
        case CustomerState::GoingToSeat:
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::Socializing;
                c.stateTimer = 8.0f + RandomInt(0, 900) / 100.0f;
            }
            break;
        case CustomerState::Socializing:
            c.stateTimer -= dt;
            if (RandomInt(0, 1000) < 2) {
                AddMess({c.pos.x + static_cast<float>(RandomInt(-12, 12)), c.pos.y + static_cast<float>(RandomInt(-12, 12))}, 0.7f);
            }
            if (c.stateTimer <= 0.0f) {
                if (c.wantsToilet && RandomInt(0, 100) < 60) {
                    c.state = CustomerState::GoingToToilet;
                    c.target = RandomPointInRect(layout_.toiletArea);
                    c.wantsToilet = false;
                } else if (c.drinksConsumed < 2 && RandomInt(0, 100) < 48) {
                    c.state = CustomerState::GoingToQueue;
                    barQueue_.push_back(c.id);
                    c.queueIndex = static_cast<int>(barQueue_.size()) - 1;
                } else {
                    c.state = CustomerState::Leaving;
                    c.target = RandomPointInRect(layout_.exitArea);
                }
            }
            break;
        case CustomerState::GoingToToilet:
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt)) {
                c.state = CustomerState::AtToilet;
                c.stateTimer = 4.0f + RandomInt(0, 300) / 100.0f;
            }
            if (toiletBlockedTimer_ > 0.0f) {
                c.mood -= 1.8f * dt;
            }
            break;
        case CustomerState::AtToilet:
            c.stateTimer -= dt;
            if (toiletBlockedTimer_ > 0.0f) {
                c.mood -= 2.8f * dt;
            }
            if (c.stateTimer <= 0.0f) {
                if (c.drinksConsumed < 2 && RandomInt(0, 100) < 35) {
                    c.state = CustomerState::GoingToQueue;
                    barQueue_.push_back(c.id);
                    c.queueIndex = static_cast<int>(barQueue_.size()) - 1;
                } else {
                    c.state = CustomerState::Leaving;
                    c.target = RandomPointInRect(layout_.exitArea);
                }
            }
            break;
        case CustomerState::Leaving:
            if (MoveTowards(c.pos, c.target, Config::CustomerMoveSpeed, dt, 8.0f)) {
                c.shouldRemove = true;
            }
            break;
    }

    if (c.mood <= 8.0f && c.state != CustomerState::Leaving) {
        c.state = CustomerState::Leaving;
        c.target = RandomPointInRect(layout_.exitArea);
    }
}

void Game::UpdateQueueTargets() {
    for (int i = 0; i < static_cast<int>(barQueue_.size()); ++i) {
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

    Vec2 serveSpot = {layout_.barCounter.x + 120, layout_.barCounter.y + layout_.barCounter.height + 16};
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

    incidentTimer_ = RandomInt(static_cast<int>(Config::IncidentIntervalMin * 100), static_cast<int>(Config::IncidentIntervalMax * 100)) / 100.0f;
    stats_.incidents += 1;

    int type = RandomInt(0, 3);
    if (type == 0) {
        Vec2 p = RandomPointInRect(layout_.standingArea);
        AddMess(p, 1.4f);
        incidentBanner_.text = "spill";
        incidentBanner_.timer = 4.0f;
    } else if (type == 1) {
        toiletBlockedTimer_ = Config::ToiletBlockDuration;
        incidentBanner_.text = "toilet";
        incidentBanner_.timer = 4.0f;
    } else if (type == 2) {
        incidentBanner_.text = "argument";
        incidentBanner_.timer = 4.0f;
        Vec2 p = RandomPointInRect(layout_.standingArea);
        for (auto& c : customers_) {
            float dx = c.pos.x - p.x;
            float dy = c.pos.y - p.y;
            if (dx * dx + dy * dy < 12000.0f) {
                c.mood -= 12.0f;
                if (RandomInt(0, 100) < 18) {
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
        incidentBanner_.text = "stock";
        incidentBanner_.timer = 4.0f;
    }
}

void Game::AddMess(Vec2 pos, float amount) {
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
    for (int i = 0; i < static_cast<int>(layout_.seats.size()); ++i) {
        if (!layout_.seats[i].occupied) return i;
    }
    return -1;
}

void Game::FreeSeat(int index) {
    if (index >= 0 && index < static_cast<int>(layout_.seats.size())) {
        layout_.seats[index].occupied = false;
    }
}

Vec2 Game::RandomPointInRect(Rect r) {
    float x = r.x + static_cast<float>(RandomInt(6, static_cast<int>(r.width) - 6));
    float y = r.y + static_cast<float>(RandomInt(6, static_cast<int>(r.height) - 6));
    return {x, y};
}

bool Game::MoveTowards(Vec2& pos, const Vec2& target, float speed, float dt, float arriveDistance) {
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

void Game::Draw(SDL_Renderer* renderer) {
    SetColor(renderer, 26, 28, 34);
    SDL_RenderClear(renderer);

    SetColor(renderer, 74, 63, 53);
    FillRect(renderer, layout_.floor);
    SetColor(renderer, 190, 170, 140);
    DrawRectOutline(renderer, layout_.floor);

    SetColor(renderer, 80, 130, 210);
    FillRect(renderer, layout_.entrance);
    SetColor(renderer, 70, 210, 130);
    FillRect(renderer, layout_.exitArea);
    SetColor(renderer, 120, 78, 42);
    FillRect(renderer, layout_.barCounter);
    SetColor(renderer, 92, 88, 84);
    FillRect(renderer, layout_.standingArea);
    if (toiletBlockedTimer_ > 0.0f) SetColor(renderer, 180, 70, 70);
    else SetColor(renderer, 110, 130, 210);
    FillRect(renderer, layout_.toiletArea);
    SetColor(renderer, 230, 230, 230, 120);
    DrawRectOutline(renderer, layout_.queueArea);

    for (const auto& s : layout_.seats) {
        if (s.occupied) SetColor(renderer, 210, 150, 70);
        else SetColor(renderer, 155, 115, 70);
        FillCenteredRect(renderer, s.pos, 24, 24);
    }

    for (const auto& q : layout_.queueSpots) {
        SetColor(renderer, 245, 245, 245, 120);
        FillCenteredRect(renderer, q, 8, 8);
    }

    for (const auto& m : messes_) {
        float size = 8.0f + m.amount * 9.0f;
        SetColor(renderer, 140, 220, 120, 220);
        FillCenteredRect(renderer, m.pos, size, size);
    }

    for (const auto& c : customers_) {
        Uint8 r = 230;
        Uint8 g = 210;
        Uint8 b = 100;
        if (c.mood < 40.0f) {
            r = 230;
            g = 120;
            b = 100;
        }
        if (c.mood > 75.0f) {
            r = 100;
            g = 220;
            b = 120;
        }
        SetColor(renderer, r, g, b);
        FillCenteredRect(renderer, c.pos, 16, 16);
        if (showOverlay_) {
            DrawBar(renderer, c.pos.x - 14.0f, c.pos.y - 22.0f, 28.0f, 5.0f, c.mood / 100.0f, 90, 230, 110);
        }
    }

    SetColor(renderer, 230, 120, 220);
    FillCenteredRect(renderer, bartender_.pos, 20, 20);
    SetColor(renderer, 120, 220, 230);
    FillCenteredRect(renderer, cleaner_.pos, 20, 20);

    Rect topPanel{0, 0, static_cast<float>(Config::ScreenWidth), 70};
    SetColor(renderer, 15, 15, 18, 220);
    FillRect(renderer, topPanel);

    DrawBar(renderer, 20, 12, 180, 16, std::clamp(stats_.revenue / 600.0f, 0.0f, 1.0f), 240, 210, 90);
    DrawBar(renderer, 220, 12, 180, 16, (stockBeer_ + stockCider_) / (Config::InitialStockBeer + Config::InitialStockCider), 120, 170, 255);
    DrawBar(renderer, 420, 12, 180, 16, std::clamp(static_cast<float>(barQueue_.size()) / 12.0f, 0.0f, 1.0f), 255, 140, 120);
    DrawBar(renderer, 620, 12, 180, 16, std::max(0.0f, timeRemaining_) / Config::DayDuration, 120, 255, 170);
    DrawBar(renderer, 820, 12, 180, 16, drinkPrice_ / 14.0f, 180, 150, 250);
    DrawBar(renderer, 1020, 12, 180, 16, std::clamp(static_cast<float>(messes_.size()) / 16.0f, 0.0f, 1.0f), 160, 255, 140);

    if (sessionState_ == SessionState::PreOpen) {
        SetColor(renderer, 10, 10, 16, 235);
        FillRect(renderer, Rect{220, 180, 840, 330});
        DrawBar(renderer, 420, 310, 440, 28, (drinkPrice_ - 2.0f) / 12.0f, 240, 220, 100);
        SetColor(renderer, 130, 230, 180, 240);
        FillRect(renderer, Rect{430, 380, 420, 58});
    }

    if (incidentBanner_.timer > 0.0f && !incidentBanner_.text.empty()) {
        SetColor(renderer, 160, 56, 56, 230);
        FillRect(renderer, Rect{330, 78, 620, 30});
    }

    if (sessionState_ == SessionState::Summary) {
        SetColor(renderer, 7, 7, 10, 240);
        FillRect(renderer, Rect{190, 120, 900, 470});
        DrawBar(renderer, 300, 220, 680, 24, std::clamp(stats_.revenue / 700.0f, 0.0f, 1.0f), 240, 210, 90);
        DrawBar(renderer, 300, 270, 680, 24, std::clamp(NetProfit() / 450.0f, 0.0f, 1.0f), 130, 230, 180);
        DrawBar(renderer, 300, 320, 680, 24, AverageSatisfaction() / 100.0f, 100, 210, 255);
        DrawBar(renderer, 300, 370, 680, 24, std::clamp(static_cast<float>(stats_.customersServed) / 40.0f, 0.0f, 1.0f), 200, 170, 255);
        DrawBar(renderer, 300, 420, 680, 24, std::clamp(static_cast<float>(stats_.incidents) / 15.0f, 0.0f, 1.0f), 255, 140, 120);
        DrawBar(renderer, 300, 470, 680, 24, std::clamp(static_cast<float>(stats_.customersUnhappy) / 30.0f, 0.0f, 1.0f), 250, 120, 120);
        SetColor(renderer, 240, 220, 110, 240);
        FillRect(renderer, Rect{470, 530, 340, 34});
    }

    SDL_RenderPresent(renderer);
}

#pragma once

namespace Config {
constexpr int ScreenWidth = 1280;
constexpr int ScreenHeight = 720;
constexpr float DayDuration = 180.0f;
constexpr float CustomerSpawnInterval = 3.0f;
constexpr float CustomerMoveSpeed = 95.0f;
constexpr float BartenderServiceBaseTime = 3.5f;
constexpr float CleanerMoveSpeed = 85.0f;
constexpr float CleanerBaseCleanTime = 2.4f;
constexpr float MoodWaitPenaltyPerSecond = 1.2f;
constexpr float MoodDirtyPenaltyPerSecond = 2.2f;
constexpr float MoodNoSeatPenalty = 8.0f;
constexpr float MoodOutOfStockPenalty = 15.0f;
constexpr float MoodHighPricePenalty = 2.0f;
constexpr float PriceBaseline = 5.0f;
constexpr float InitialDrinkPrice = 5.0f;
constexpr float InitialStockBeer = 70.0f;
constexpr float InitialStockCider = 50.0f;
constexpr float BaseWageBartender = 120.0f;
constexpr float BaseWageCleaner = 90.0f;
constexpr float IncidentIntervalMin = 10.0f;
constexpr float IncidentIntervalMax = 22.0f;
constexpr float ToiletBlockDuration = 18.0f;
constexpr int MaxCustomers = 60;
}

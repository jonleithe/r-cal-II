#include "RCalHeaderFramework.h"
// #include "CalendarStore.h"
// #include "Scheduling.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cwchar>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <curses.h>

namespace rcal{
namespace{

std::string trim(std::string value){
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}
// ——— End of trim() ———————————————————————————————————————————————————————————



std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
// ——— End of lower() ——————————————————————————————————————————————————————————



std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::istringstream input(value);
    std::string item;
    while (std::getline(input, item, delimiter)) result.push_back(trim(item));
    return result;
}
// ——— End of split() ——————————————————————————————————————————————————————————

long parseLong(const std::string& value, const std::string& key, long minimum,
               long maximum) {
    char* end = nullptr;
    errno = 0;
    const long number = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0' || number < minimum ||
        number > maximum) {
        throw std::runtime_error("invalid value for '" + key + "': " + value);
    }
    return number;
}
// ——— End of parseLong() ——————————————————————————————————————————————————————

bool parseBool(const std::string& value, const std::string& key) {
    const std::string normalized = lower(trim(value));
    if (normalized == "true" || normalized == "yes" || normalized == "1") return true;
    if (normalized == "false" || normalized == "no" || normalized == "0") return false;
    throw std::runtime_error("invalid boolean for '" + key + "': " + value);
}
// ——— End of parseBool() ——————————————————————————————————————————————————————

Date parseDate(const std::string& value) {
    const auto parts = split(value, '-');
    if (parts.size() != 3) throw std::runtime_error("start_date must be YYYY-MM-DD");
    return {
        static_cast<int>(parseLong(parts[0], "start_date year", 1, 9999)),
        static_cast<unsigned>(parseLong(parts[1], "start_date month", 1, 12)),
        static_cast<unsigned>(parseLong(parts[2], "start_date day", 1, 31))
    };
}
// ——— End of parseDate() ——————————————————————————————————————————————————————

bool leapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
// ——— End of leapYear() ———————————————————————————————————————————————————————

unsigned daysInMonth(int year, unsigned month) {
    static constexpr std::array<unsigned, 12> days{{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    }};
    return days.at(month - 1) + (month == 2 && leapYear(year) ? 1U : 0U);
}
// ——— End of daysInMonth() ————————————————————————————————————————————————————

void validateDate(const Date& date) {
    if (date.year < 1 || date.year > 9999 || date.month < 1 || date.month > 12 ||
        date.day < 1 || date.day > daysInMonth(date.year, date.month)) {
        throw std::runtime_error("invalid calendar date");
    }
}
// ——— End of validateDate() ———————————————————————————————————————————————————

long long daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear =
        (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 +
                              dayOfYear;
    return static_cast<long long>(era) * 146097 + dayOfEra - 719468;
}
// ——— End of daysFromCivil() ——————————————————————————————————————————————————

Date civilFromDays(long long days) {
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
    const unsigned yearOfEra =
        (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    int year = static_cast<int>(yearOfEra) + static_cast<int>(era) * 400;
    const unsigned dayOfYear = dayOfEra -
        (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned monthPart = (5 * dayOfYear + 2) / 153;
    const unsigned day = dayOfYear - (153 * monthPart + 2) / 5 + 1;
    const unsigned month = monthPart < 10 ? monthPart + 3 : monthPart - 9;
    year += month <= 2;
    return {year, month, day};
}
// ——— End of civilFromDays() ——————————————————————————————————————————————————

unsigned isoWeekday(long long days) {
    long long weekday = (days + 3) % 7;
    if (weekday < 0) weekday += 7;
    return static_cast<unsigned>(weekday + 1);
}
// ——— End of isoWeekday() —————————————————————————————————————————————————————

unsigned ordinalDay(const Date& date) {
    static constexpr std::array<unsigned, 12> offsets{{
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    }};
    return offsets[date.month - 1] + date.day +
           (date.month > 2 && leapYear(date.year) ? 1U : 0U);
}
// ——— End of ordinalDay() —————————————————————————————————————————————————————

unsigned isoWeeksInYear(int year) {
    const unsigned jan1 = isoWeekday(daysFromCivil(year, 1, 1));
    return jan1 == 4 || (jan1 == 3 && leapYear(year)) ? 53U : 52U;
}
// ——— End of isoWeeksInYear() —————————————————————————————————————————————————

unsigned isoWeek(const Date& date, unsigned weekday) {
    int week = (static_cast<int>(ordinalDay(date)) - static_cast<int>(weekday) + 10) / 7;
    if (week < 1) return isoWeeksInYear(date.year - 1);
    if (week > static_cast<int>(isoWeeksInYear(date.year))) return 1;
    return static_cast<unsigned>(week);
}
// ——— End of isoWeek() ————————————————————————————————————————————————————————

Date currentLocalDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1),
            static_cast<unsigned>(local.tm_mday)};
}
// ——— End of currentLocalDate() ———————————————————————————————————————————————

bool sameDate(const Date& left, const Date& right) {
    return left.year == right.year && left.month == right.month && left.day == right.day;
}
// ——— End of sameDate() ———————————————————————————————————————————————————————

Date addDays(const Date& date, long long days) {
    return civilFromDays(daysFromCivil(date.year, date.month, date.day) + days);
}
// ——— End of addDays() ————————————————————————————————————————————————————————

Date gregorianEasterSunday(int year) {
    const int a = year % 19;
    const int b = year / 100;
    const int c = year % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const unsigned month = static_cast<unsigned>((h + l - 7 * m + 114) / 31);
    const unsigned day = static_cast<unsigned>((h + l - 7 * m + 114) % 31 + 1);
    return {year, month, day};
}
// ——— End of gregorianEasterSunday() ——————————————————————————————————————————

std::string norwegianHolidayName(const Date& date) {
    if (date.month == 1 && date.day == 1) return "Nyttårsdag";
    if (date.month == 5 && date.day == 1) return "Arbeidernes dag";
    if (date.month == 5 && date.day == 17) return "Grunnlovsdagen";
    if (date.month == 12 && date.day == 25) return "Første juledag";
    if (date.month == 12 && date.day == 26) return "Andre juledag";

    const Date easterSunday = gregorianEasterSunday(date.year);
    const long long easter = daysFromCivil(date.year, easterSunday.month, easterSunday.day);
    const long long offset = daysFromCivil(date.year, date.month, date.day) - easter;
    switch (offset) {
        case -3: return "Skjærtorsdag";
        case -2: return "Langfredag";
        case 0: return "Første påskedag";
        case 1: return "Andre påskedag";
        case 39: return "Kristi himmelfartsdag";
        case 49: return "Første pinsedag";
        case 50: return "Andre pinsedag";
        default: return {};
    }
}
// ——— End of norwegianHolidayName() ———————————————————————————————————————————

std::string holidayName(const Date& date, const std::string& country) {
    if (lower(country) == "no") return norwegianHolidayName(date);
    return {};
}
// ——— End of holidayName() ————————————————————————————————————————————————————

ColorStyle parseStyle(const std::string& value) {
    const auto parts = split(value, ',');
    ColorStyle style;
    if (!parts.empty() && !parts[0].empty()) style.foreground = lower(parts[0]);
    if (parts.size() > 1 && !parts[1].empty()) style.background = lower(parts[1]);
    for (std::size_t i = 2; i < parts.size(); ++i) {
        const std::string flag = lower(parts[i]);
        if (flag == "bold") style.bold = true;
        else if (flag == "underline") style.underline = true;
        else if (flag == "reverse") style.reverse = true;
        else if (!flag.empty()) throw std::runtime_error("unknown color attribute: " + flag);
    }
    return style;
}
// ——— End of parseStyle() —————————————————————————————————————————————————————

ColorStyle selectedStyle(const ColorStyle& base, const ColorStyle& highlight) {
    ColorStyle result = base;
    result.background = highlight.background;
    result.bold = result.bold || highlight.bold;
    result.underline = result.underline || highlight.underline;
    result.reverse = result.reverse || highlight.reverse;
    return result;
}
// ——— End of selectedStyle() ——————————————————————————————————————————————————

short cursesColor(const std::string& name) {
    static const std::map<std::string, short> names{{
        {"default", -1}, {"black", COLOR_BLACK}, {"red", COLOR_RED},
        {"green", COLOR_GREEN}, {"yellow", COLOR_YELLOW}, {"blue", COLOR_BLUE},
        {"magenta", COLOR_MAGENTA}, {"cyan", COLOR_CYAN}, {"white", COLOR_WHITE}
    }};
    const auto found = names.find(lower(name));
    if (found != names.end()) return found->second;
    return static_cast<short>(parseLong(name, "color", 0, SHRT_MAX));
}
// ——— End of cursesColor() ————————————————————————————————————————————————————

struct StyleRegistry {
    short nextPair = 1;
    std::map<std::pair<short, short>, short> pairs;

    attr_t attributes(const ColorStyle& style) {
        attr_t result = A_NORMAL;
        if (style.bold) result |= A_BOLD;
        if (style.underline) result |= A_UNDERLINE;
        if (style.reverse) result |= A_REVERSE;
        if (!has_colors()) return result;
        const short foreground = cursesColor(style.foreground);
        const short background = cursesColor(style.background);
        const auto key = std::make_pair(foreground, background);
        auto found = pairs.find(key);
        if (found == pairs.end()) {
            if (nextPair >= COLOR_PAIRS) return result;
            init_pair(nextPair, foreground, background);
            found = pairs.emplace(key, nextPair++).first;
        }
        return result | COLOR_PAIR(found->second);
    }
    // ——— End of attributes() —————————————————————————————————————————————————
};

int characterWidth(wchar_t character) {
    const int width = ::wcwidth(character);
    return width < 0 ? 1 : width;
}
// ——— End of characterWidth() —————————————————————————————————————————————————

std::wstring toWide(const std::string& text) {
    std::wstring result;
    result.reserve(text.size());
    std::mbstate_t state{};
    const char* position = text.data();
    std::size_t remaining = text.size();
    while (remaining > 0) {
        wchar_t character = L'\0';
        const std::size_t length = std::mbrtowc(&character, position, remaining, &state);
        if (length == static_cast<std::size_t>(-1) ||
            length == static_cast<std::size_t>(-2)) {
            throw std::runtime_error("invalid UTF-8 text in calendar configuration");
        }
        if (length == 0) break;
        result.push_back(character);
        position += length;
        remaining -= length;
    }
    return result;
}
// ——— End of toWide() —————————————————————————————————————————————————————————

std::wstring fitToWidth(const std::wstring& text, unsigned width) {
    std::wstring result;
    unsigned used = 0;
    for (wchar_t character : text) {
        const unsigned characterColumns = static_cast<unsigned>(characterWidth(character));
        if (used + characterColumns > width) break;
        result.push_back(character);
        used += characterColumns;
    }
    return result;
}
// ——— End of fitToWidth() —————————————————————————————————————————————————————

unsigned displayWidth(const std::wstring& text) {
    unsigned result = 0;
    for (wchar_t character : text)
        result += static_cast<unsigned>(characterWidth(character));
    return result;
}
// ——— End of displayWidth() ———————————————————————————————————————————————————

std::wstring centered(const std::string& text, unsigned width) {
    const std::wstring fitted = fitToWidth(toWide(text), width);
    const unsigned textWidth = displayWidth(fitted);
    const unsigned left = (width - textWidth) / 2;
    return std::wstring(left, L' ') + fitted +
           std::wstring(width - left - textWidth, L' ');
}
// ——— End of centered() ———————————————————————————————————————————————————————

void put(WINDOW* window, int y, int x, const std::wstring& text, attr_t attributes) {
    int windowHeight = 0;
    int windowWidth = 0;
    getmaxyx(window, windowHeight, windowWidth);
    if (y < 0 || y >= windowHeight || x >= windowWidth || text.empty()) return;

    std::size_t offset = 0;
    if (x < 0) {
        int columnsToSkip = -x;
        while (offset < text.size() && columnsToSkip > 0) {
            columnsToSkip -= characterWidth(text[offset]);
            ++offset;
        }
        x = 0;
    }
    const unsigned available = static_cast<unsigned>(windowWidth - x);
    const std::wstring visible = fitToWidth(text.substr(offset), available);
    if (visible.empty()) return;

    wattrset(window, attributes);
    mvwaddnwstr(window, y, x, visible.c_str(), static_cast<int>(visible.size()));
}
// ——— End of put() ————————————————————————————————————————————————————————————

void put(WINDOW* window, int y, int x, const std::string& text, attr_t attributes) {
    put(window, y, x, toWide(text), attributes);
}
// ——— End of put() ————————————————————————————————————————————————————————————

} // namespace

ColorStyle highlightedStyle(const ColorStyle& base, const ColorStyle& highlight) {
    return selectedStyle(base, highlight);
}
// ——— End of highlightedStyle() ———————————————————————————————————————————————

RCalHeaderFramework::RCalHeaderFramework() {
    options_.startDate = currentLocalDate();
    rebuildTimeline();
}
// ——— End of RCalHeaderFramework() ————————————————————————————————————————————

RCalHeaderFramework::RCalHeaderFramework(HeaderOptions options)
    : options_(std::move(options)) { rebuildTimeline(); }
// ——— End of RCalHeaderFramework() ————————————————————————————————————————————

RCalHeaderFramework RCalHeaderFramework::fromConfigFile(const std::string& path) {
    RCalHeaderFramework result;
    result.loadConfigFile(path);
    return result;
}
// ——— End of fromConfigFile() —————————————————————————————————————————————————

void RCalHeaderFramework::loadConfigFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open config file: " + path);
    HeaderOptions newOptions = options_;
    HeaderColors newColors = colors_;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string clean = trim(line);
        if (clean.empty() || clean[0] == '#' || clean[0] == ';') continue;
        const auto equals = clean.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(lineNumber) +
                                     ": expected key=value");
        }
        const std::string key = lower(trim(clean.substr(0, equals)));
        const std::string value = trim(clean.substr(equals + 1));
        try {
            if (key == "start_date") newOptions.startDate =
                lower(value) == "today" ? currentLocalDate() : parseDate(value);
            else if (key == "days") newOptions.dayCount =
                static_cast<std::size_t>(parseLong(value, key, 1, 3660));
            else if (key == "buffer_days") newOptions.bufferDays =
                static_cast<std::size_t>(parseLong(value, key, 1, 3660));
            else if (key == "rebuild_threshold_days") newOptions.rebuildThresholdDays =
                static_cast<std::size_t>(parseLong(value, key, 1, 3659));
            else if (key == "cell_width") newOptions.cellWidth =
                static_cast<unsigned>(parseLong(value, key, 5, 20));
            else if (key == "show_grid") newOptions.showGrid = parseBool(value, key);
            else if (key == "data_file") newOptions.dataFile = value;
            else if (key == "holiday_country"){
                const std::string country = lower(value);
                if (country == "no") newOptions.holidayCountry = "NO";
                else if (country == "none") newOptions.holidayCountry = "none";
                else throw std::runtime_error("unsupported holiday country '" + value + "'");
            }

            else if (key == "month_names"){
                const auto names = split(value, ',');
                if (names.size() != 12) throw std::runtime_error("month_names needs 12 values");
                std::copy(names.begin(), names.end(), newOptions.monthNames.begin());
            }
            else if (key == "day_names"){
                const auto names = split(value, ',');
                if (names.size() != 7) throw std::runtime_error("day_names needs 7 values");
                std::copy(names.begin(), names.end(), newOptions.dayNames.begin());
            }
            else if (key == "label.year") newOptions.yearLabel = value;
            else if (key == "label.month") newOptions.monthLabel = value;
            else if (key == "label.week") newOptions.weekLabel = value;
            else if (key == "label.day_of_year") newOptions.dayOfYearLabel = value;
            else if (key == "label.date") newOptions.dateLabel = value;
            else if (key == "label.day") newOptions.dayLabel = value;
            else if (key == "label.time_info") newOptions.timeInfoLabel = value;
            else if (key == "label.details") newOptions.detailsLabel = value;
            else if (key == "label.holiday_info") newOptions.holidayInfoPrefix = value;
            else if (key == "color.label") newColors.label = parseStyle(value);
            else if (key == "color.year") newColors.year = parseStyle(value);
            else if (key == "color.month") newColors.month = parseStyle(value);
            else if (key == "color.week_number") newColors.weekNumber = parseStyle(value);
            else if (key == "color.day_of_year") newColors.dayOfYear = parseStyle(value);
            else if (key == "color.date") newColors.date = parseStyle(value);
            else if (key == "color.day_name") newColors.dayName = parseStyle(value);
            else if (key == "color.weekend") newColors.weekend = parseStyle(value);
            else if (key == "color.holiday") newColors.holiday = parseStyle(value);
            else if (key == "color.current_day") newColors.currentDay = parseStyle(value);
            else if (key == "color.selected_item") newColors.selectedItem = parseStyle(value);
            else if (key == "color.selected_day") newColors.selectedDay = parseStyle(value);
            else if (key == "color.notes") newColors.notes = parseStyle(value);
            else if (key == "color.overdue_task") newColors.overdueTask = parseStyle(value);
            else if (key == "color.tasks") newColors.tasks = parseStyle(value);
            else if (key == "color.events") newColors.events = parseStyle(value);
            else if (key == "color.time_info") newColors.timeInfo = parseStyle(value);
            else if (key == "color.grid") newColors.grid = parseStyle(value);
            else if (key == "color.status_bar") newColors.statusBar = parseStyle(value);
            else throw std::runtime_error("unknown setting '" + key + "'");
        } catch (const std::exception& error) {
            throw std::runtime_error(path + ":" + std::to_string(lineNumber) + ": " + error.what());
        }
    }
    options_ = std::move(newOptions);
    colors_ = std::move(newColors);
    rebuildTimeline();
}
// ——— End of loadConfigFile() —————————————————————————————————————————————————



const HeaderOptions& RCalHeaderFramework::options() const noexcept { return options_; }
// ——— End of options() ————————————————————————————————————————————————————————
const HeaderColors& RCalHeaderFramework::colors() const noexcept { return colors_; }
// ——— End of colors() —————————————————————————————————————————————————————————
const std::vector<TimelineDay>& RCalHeaderFramework::timeline() const noexcept { return timeline_; }
// ——— End of timeline() ———————————————————————————————————————————————————————
const Date& RCalHeaderFramework::selectedDate() const noexcept { return selectedDate_; }
// ——— End of selectedDate() ———————————————————————————————————————————————————
const std::string& RCalHeaderFramework::selectedHolidayName() const noexcept {
    static const std::string empty;
    if (selectedDayIndex_ >= timeline_.size()) return empty;
    const auto& selected = timeline_[selectedDayIndex_];
    return sameDate(selected.date, selectedDate_) ? selected.holidayName : empty;
}
// ——— End of selectedHolidayName() ————————————————————————————————————————————
std::size_t RCalHeaderFramework::firstVisibleDay() const noexcept { return firstVisibleDay_; }
// ——— End of firstVisibleDay() ————————————————————————————————————————————————
std::size_t RCalHeaderFramework::visibleDayCount() const noexcept { return visibleDayCount_; }
// ——— End of visibleDayCount() ————————————————————————————————————————————————



// Disabled for the calendar-only design preview.
// const DayInfo* RCalHeaderFramework::dayInfo(Date date) const noexcept {
//     const auto found = dayInformation_.find(daysFromCivil(date.year, date.month, date.day));
//     return found == dayInformation_.end() ? nullptr : &found->second;
// }
// // ——— End of dayInfo() ————————————————————————————————————————————————————————
// 
// const std::string* RCalHeaderFramework::selectedNote() const noexcept {
//     const DayInfo* info = dayInfo(selectedDate_);
//     return info && selectedItemType_ == DayItemType::note &&
//         selectedItemIndex_ < info->notes.size()
//         ? &info->notes[selectedItemIndex_].text : nullptr;
// }
// 
// std::size_t RCalHeaderFramework::selectedNoteIndex() const noexcept {
//     return selectedItemType_ == DayItemType::note ? selectedItemIndex_ : 0;
// }
// 
// DayItemType RCalHeaderFramework::selectedItemType() const noexcept {
//     return selectedItemType_;
// }
// 
// std::size_t RCalHeaderFramework::selectedItemIndex() const noexcept {
//     return selectedItemIndex_;
// }
// 
// std::size_t RCalHeaderFramework::selectedItemFlatIndex() const noexcept {
//     const DayInfo* info = dayInfo(selectedDate_);
//     return info ? selectedFlatIndex(*info) : 0;
// }
// 
// std::string RCalHeaderFramework::selectedItemText() const {
//     const DayInfo* info = dayInfo(selectedDate_);
//     if (!info) return {};
//     if (selectedItemType_ == DayItemType::note && selectedItemIndex_ < info->notes.size())
//         return info->notes[selectedItemIndex_].text;
//     if (selectedItemType_ == DayItemType::task && selectedItemIndex_ < info->tasks.size()) {
//         const Task& task = info->tasks[selectedItemIndex_];
//         return scheduledText(task.dueDate, task.dueTime, task.text, true);
//     }
//     if (selectedItemType_ == DayItemType::event && selectedItemIndex_ < info->events.size()) {
//         const Event& event = info->events[selectedItemIndex_];
//         return scheduledText(event.startDate, event.time, event.text, false);
//     }
//     return {};
// }
// 
// 
// 
// std::string RCalHeaderFramework::infoIndicator(Date date) const {
//     validateDate(date);
//     std::string result(5, ' ');
//     const DayInfo* info = dayInfo(date);
//     if (info) {
//         switch (info->workStatus) {
//             case WorkStatus::workday: result[0] = 'W'; break;
//             case WorkStatus::travelling: result[0] = 'T'; break;
//             case WorkStatus::outOfOfficeWorking: result[0] = 'O'; break;
//             case WorkStatus::compensatoryLeave: result[0] = 'A'; break;
//             case WorkStatus::vacation: result[0] = 'F'; break;
//             case WorkStatus::unspecified: break;
//         }
//         if (!info->notes.empty()) result[2] = 'N';
//         const unsigned taskCount = std::max(info->taskCount,
//             static_cast<unsigned>(info->tasks.size()));
//         const unsigned eventCount = std::max(info->eventCount,
//             static_cast<unsigned>(info->events.size()));
//         if (taskCount > 0)
//             result[3] = taskCount < 10 ? static_cast<char>('0' + taskCount) : '+';
//         if (eventCount > 0) result[4] = 'E';
//     }
//     if (!holidayName(date, options_.holidayCountry).empty()) result[1] = 'H';
//     return result;
// }
// // ——— End of infoIndicator() ——————————————————————————————————————————————————
// 
// 
// 
// std::string RCalHeaderFramework::infoDescription(Date date) const {
//     validateDate(date);
//     std::vector<std::string> details;
//     const DayInfo* info = dayInfo(date);
//     if (info) {
//         switch (info->workStatus) {
//             case WorkStatus::workday: details.push_back("W Workday"); break;
//             case WorkStatus::travelling: details.push_back("T Travelling"); break;
//             case WorkStatus::outOfOfficeWorking:
//                 details.push_back("O Out-of-office working");
//                 break;
//             case WorkStatus::compensatoryLeave:
//                 details.push_back("A Compensatory leave");
//                 break;
//             case WorkStatus::vacation: details.push_back("F Vacation"); break;
//             case WorkStatus::unspecified: break;
//         }
//     }
// 
//     const std::string holiday = holidayName(date, options_.holidayCountry);
//     if (!holiday.empty()) details.push_back("H " + holiday);
//     if (info && info->notes.size() == 1) details.push_back("N " + info->notes.front().text);
//     else if (info && !info->notes.empty())
//         details.push_back("N " + std::to_string(info->notes.size()) + " notes");
//     const unsigned taskCount = info ? std::max(info->taskCount,
//         static_cast<unsigned>(info->tasks.size())) : 0;
//     const unsigned eventCount = info ? std::max(info->eventCount,
//         static_cast<unsigned>(info->events.size())) : 0;
//     if (taskCount > 0)
//         details.push_back(std::to_string(taskCount) + (taskCount == 1 ? " task" : " tasks"));
//     if (eventCount > 0)
//         details.push_back(std::to_string(eventCount) + (eventCount == 1 ? " event" : " events"));
// 
//     if (details.empty()) return "No day information";
//     std::string result;
//     for (const std::string& detail : details) {
//         if (!result.empty()) result += " | ";
//         result += detail;
//     }
//     return result;
// }
// // ——— End of infoDescription() ———————————————————————————————————————————————
// 
// 
// 
std::string RCalHeaderFramework::infoIndicator(Date date) const {
    return holidayName(date, options_.holidayCountry).empty() ? "" : " H";
}

std::string RCalHeaderFramework::infoDescription(Date date) const {
    const auto holiday = holidayName(date, options_.holidayCountry);
    return holiday.empty() ? "Left/Right: day  PgUp/PgDn: page  Home/n: today  q: quit" : holiday;
}

void RCalHeaderFramework::setOptions(HeaderOptions options) {
    options_ = std::move(options);
    rebuildTimeline();
}
// ——— End of setOptions() —————————————————————————————————————————————————————



void RCalHeaderFramework::setColors(HeaderColors colors)
{
    colors_ = std::move(colors);
}
// ——— End of setColors() ——————————————————————————————————————————————————————



// Disabled for the calendar-only design preview.
// void RCalHeaderFramework::setCalendarStore(std::shared_ptr<CalendarStore> store) {
//     calendarStore_ = std::move(store);
//     memoryEvents_.clear();
//     dayInformation_.clear();
//     selectedItemType_ = DayItemType::note;
//     selectedItemIndex_ = 0;
//     loadTimelineDayInformation();
// }
// 
// 
// void RCalHeaderFramework::setDayInfo(Date date, DayInfo info) {
//     validateDate(date);
//     dayInformation_[daysFromCivil(date.year, date.month, date.day)] = info;
// }
// // ——— End of setDayInfo() —————————————————————————————————————————————————————
// 
// 
// 
// void RCalHeaderFramework::addNote(Date date, std::string note) {
//     validateDate(date);
//     if (note.empty()) return;
//     loadDayInfo(date);
//     DayInfo& info = dayInformation_[daysFromCivil(date.year, date.month, date.day)];
//     if (calendarStore_)
//         info.notes.push_back(calendarStore_->addNote(date, std::move(note), info.notes.size()));
//     else
//         info.notes.push_back({{}, std::move(note)});
//     if (sameDate(date, selectedDate_)) {
//         selectedItemType_ = DayItemType::note;
//         selectedItemIndex_ = info.notes.size() - 1;
//     }
// }
// // ——— End of addNote() ————————————————————————————————————————————————————————
// 
// 
// 
// const Task* RCalHeaderFramework::selectedTask() const noexcept {
//     const auto* info = dayInfo(selectedDate_);
//     return info && selectedItemType_ == DayItemType::task && selectedItemIndex_ < info->tasks.size()
//         ? &info->tasks[selectedItemIndex_] : nullptr;
// }
// 
// void RCalHeaderFramework::addTask(Date date, std::string text) {
//     const auto parts = parseScheduledText(text, true);
//     addTask(date, Task{{}, parts.text, false, parts.date, parts.time});
// }
// 
// void RCalHeaderFramework::addTask(Date date, Task task) {
//     validateDate(date);
//     if (task.text.empty()) return;
//     validateSchedule(task.dueDate, task.dueTime);
//     if (task.dueDate.empty() && !task.dueTime.empty())
//         throw std::invalid_argument("task due time requires a due date");
//     loadDayInfo(date);
//     DayInfo& info = dayInformation_[daysFromCivil(date.year, date.month, date.day)];
//     if (calendarStore_)
//         info.tasks.push_back(calendarStore_->addTask(date, task.text, info.tasks.size(), task.dueDate, task.dueTime));
//     else
//         info.tasks.push_back({{}, task.text, false, task.dueDate, task.dueTime});
//     if (sameDate(date, selectedDate_)) {
//         selectedItemType_ = DayItemType::task;
//         selectedItemIndex_ = info.tasks.size() - 1;
//     }
// }
// 
// void RCalHeaderFramework::updateSelectedTask(Task task) {
//     if (!selectedTask()) return;
//     if (!task.text.empty()) {
//         validateSchedule(task.dueDate, task.dueTime);
//         if (task.dueDate.empty() && !task.dueTime.empty())
//             throw std::invalid_argument("task due time requires a due date");
//     }
//     const auto key = daysFromCivil(selectedDate_.year, selectedDate_.month, selectedDate_.day);
//     auto found = dayInformation_.find(key);
//     auto& info = found->second;
//     auto& selected = info.tasks[selectedItemIndex_];
//     if (task.text.empty()) {
//         if (calendarStore_) calendarStore_->deleteTask(selected.id);
//         info.tasks.erase(info.tasks.begin() + static_cast<std::ptrdiff_t>(selectedItemIndex_));
//         if (itemCount(info)) selectFlatIndex(info, 0);
//         else { selectedItemType_ = DayItemType::note; selectedItemIndex_ = 0; }
//         discardEmptyDayInfo(selectedDate_);
//     } else {
//         if (calendarStore_) calendarStore_->updateTask(selected.id, task.text, task.dueDate, task.dueTime);
//         // Editing scheduling or text preserves the record identity and completion state.
//         selected.text = task.text;
//         selected.dueDate = task.dueDate;
//         selected.dueTime = task.dueTime;
//     }
// }
// 
// const Event* RCalHeaderFramework::selectedEvent() const noexcept {
//     const auto* info = dayInfo(selectedDate_);
//     return info && selectedItemType_ == DayItemType::event && selectedItemIndex_ < info->events.size()
//         ? &info->events[selectedItemIndex_] : nullptr;
// }
// 
// void RCalHeaderFramework::refreshEventViews(const EventId& id, const Event* event) {
//     // Drop every cached occurrence before rebuilding views from the single record.
//     for (auto& day : dayInformation_) {
//         auto& events = day.second.events;
//         events.erase(std::remove_if(events.begin(), events.end(),
//             [&](const Event& existing) { return existing.id == id; }), events.end());
//     }
//     if (!calendarStore_) {
//         memoryEvents_.erase(id);
//         if (event) memoryEvents_[id] = *event;
//     }
//     std::vector<Date> dates;
//     for (const auto& day : dayInformation_) dates.push_back(civilFromDays(day.first));
//     for (const auto& day : timeline_) dates.push_back(day.date);
//     dates.push_back(selectedDate_);
//     if (event) {
//         dates.push_back(parseDate(event->startDate));
//         dates.push_back(parseDate(event->endDate));
//     }
//     for (const auto& date : dates) loadDayInfo(date);
// }
// 
// void RCalHeaderFramework::addEvent(Date date, std::string text) {
//     const auto parts = parseScheduledText(text, false);
//     if (parts.text.empty()) return;
//     addEvent(date, Event{{}, parts.time, parts.text, parts.date, {}, {}});
// }
// 
// void RCalHeaderFramework::addEvent(Date date, Event event) {
//     validateDate(date);
//     if (event.text.empty()) return;
//     normalizeEvent(event, calendarDateString(date));
//     if (calendarStore_) {
//         const auto* info = dayInfo(date);
//         event = calendarStore_->addEvent(date, event.time, event.text,
//             info ? info->events.size() : 0, event.startDate, event.endDate, event.endTime);
//     } else {
//         event.id = "memory-event-" + std::to_string(++nextEventId_);
//     }
//     refreshEventViews(event.id, &event);
//     if (!eventOnDate(event, calendarDateString(selectedDate_))) selectDate(parseDate(event.startDate));
//     const auto* info = dayInfo(selectedDate_);
//     if (info) {
//         for (std::size_t i = 0; i < info->events.size(); ++i) {
//             if (info->events[i].id == event.id) {
//                 selectedItemType_ = DayItemType::event;
//                 selectedItemIndex_ = i;
//                 break;
//             }
//         }
//     }
// }
// 
// void RCalHeaderFramework::updateSelectedEvent(Event event) {
//     const auto* selected = selectedEvent();
//     if (!selected) return;
//     const Event previous = *selected;
//     event.id = previous.id;
//     if (event.text.empty()) {
//         if (calendarStore_) calendarStore_->deleteEvent(event.id);
//         refreshEventViews(event.id, nullptr);
//     } else {
//         normalizeEvent(event, previous.startDate);
//         if (calendarStore_) calendarStore_->updateEvent(event.id, event.time, event.text,
//             event.startDate, event.endDate, event.endTime);
//         refreshEventViews(event.id, &event);
//     }
//     const auto* info = dayInfo(selectedDate_);
//     if (info && itemCount(*info)) {
//         for (std::size_t i = 0; i < info->events.size(); ++i) {
//             if (info->events[i].id == event.id) {
//                 selectedItemType_ = DayItemType::event;
//                 selectedItemIndex_ = i;
//                 return;
//             }
//         }
//         selectFlatIndex(*info, 0);
//     } else {
//         selectedItemType_ = DayItemType::note;
//         selectedItemIndex_ = 0;
//     }
// }
// 
// void RCalHeaderFramework::updateSelectedNote(std::string note) {
//     const long long key = daysFromCivil(selectedDate_.year, selectedDate_.month,
//                                         selectedDate_.day);
//     auto found = dayInformation_.find(key);
//     if (found == dayInformation_.end() || selectedItemType_ != DayItemType::note ||
//         selectedItemIndex_ >= found->second.notes.size())
//         return;
//     if (note.empty()) {
//         if (calendarStore_) calendarStore_->deleteNote(found->second.notes[selectedItemIndex_].id);
//         found->second.notes.erase(found->second.notes.begin() +
//                                  static_cast<std::ptrdiff_t>(selectedItemIndex_));
//         if (selectedItemIndex_ >= found->second.notes.size() && selectedItemIndex_ > 0)
//             --selectedItemIndex_;
//     } else {
//         if (calendarStore_)
//             calendarStore_->updateNote(found->second.notes[selectedItemIndex_].id, note);
//         found->second.notes[selectedItemIndex_].text = std::move(note);
//     }
//     const DayInfo& info = found->second;
//     if (info.workStatus == WorkStatus::unspecified && info.notes.empty() && info.tasks.empty() &&
//         info.events.empty() && info.taskCount == 0 && info.eventCount == 0)
//         dayInformation_.erase(found);
//     else if (selectedItemIndex_ >= info.notes.size() && itemCount(info) > 0)
//         selectFlatIndex(info, std::min(selectedItemIndex_, itemCount(info) - 1));
// }
// // ——— End of updateSelectedNote() —————————————————————————————————————————————
// 
// 
// 
// void RCalHeaderFramework::updateSelectedItem(std::string text) {
//     if (selectedItemType_ == DayItemType::event) {
//         const auto* selected = selectedEvent();
//         if (!selected) return;
//         Event event = *selected;
//         const auto parts = parseScheduledText(text, false);
//         event.text = parts.text;
//         event.time = parts.time;
//         if (!parts.date.empty() && parts.date != event.startDate) {
//             event.startDate = parts.date;
//             event.endDate = parts.date;
//         }
//         updateSelectedEvent(event);
//         return;
//     }
//     if (selectedItemType_ == DayItemType::note) {
//         updateSelectedNote(std::move(text));
//         return;
//     }
//     const auto parts = parseScheduledText(text, true);
//     if (!text.empty() && parts.text.empty()) throw std::invalid_argument("task description is required");
//     updateSelectedTask(Task{{}, parts.text, false, parts.date, parts.time});
// }
// 
// bool RCalHeaderFramework::deleteSelectedItem() {
//     if (selectedItemText().empty()) return false;
//     const auto* info = dayInfo(selectedDate_);
//     const auto index = selectedFlatIndex(*info);
//     updateSelectedItem("");
//     info = dayInfo(selectedDate_);
//     if (info && itemCount(*info)) selectFlatIndex(*info, std::min(index, itemCount(*info) - 1));
//     return true;
// }
// 
// void RCalHeaderFramework::moveNoteSelection(int rows) {
//     const DayInfo* info = dayInfo(selectedDate_);
//     if (!info || itemCount(*info) == 0 || rows == 0) return;
//     const long long candidate = static_cast<long long>(selectedFlatIndex(*info)) + rows;
//     selectFlatIndex(*info, static_cast<std::size_t>(std::max(0LL, std::min(
//         static_cast<long long>(itemCount(*info) - 1), candidate))));
// }
// // ——— End of moveNoteSelection() ——————————————————————————————————————————————
// 
// void RCalHeaderFramework::toggleSelectedTask() {
//     const auto found = dayInformation_.find(daysFromCivil(selectedDate_.year, selectedDate_.month,
//                                                            selectedDate_.day));
//     if (found == dayInformation_.end() || selectedItemType_ != DayItemType::task ||
//         selectedItemIndex_ >= found->second.tasks.size()) return;
//     Task& task = found->second.tasks[selectedItemIndex_];
//     task.completed = !task.completed;
//     if (calendarStore_) calendarStore_->setTaskCompleted(task.id, task.completed);
// }
// 
// 
// 
// bool RCalHeaderFramework::removeDayInfo(Date date) {
//     validateDate(date);
//     return dayInformation_.erase(daysFromCivil(date.year, date.month, date.day)) > 0;
// }
// // ——— End of removeDayInfo() ——————————————————————————————————————————————————
// 
// 
// 
// void RCalHeaderFramework::toggleSelectedWorkStatus(WorkStatus status) {
//     if (status == WorkStatus::unspecified) {
//         clearSelectedWorkStatus();
//         return;
//     }
//     DayInfo info;
//     if (const DayInfo* existing = dayInfo(selectedDate_)) info = *existing;
//     info.workStatus = info.workStatus == status ? WorkStatus::unspecified : status;
//     if (calendarStore_) calendarStore_->saveWorkStatus(selectedDate_, info.workStatus);
//     setDayInfo(selectedDate_, info);
//     discardEmptyDayInfo(selectedDate_);
// }
// // ——— End of toggleSelectedWorkStatus() ———————————————————————————————————————
// 
// 
// 
// void RCalHeaderFramework::clearSelectedWorkStatus() {
//     DayInfo* info = nullptr;
//     const auto found = dayInformation_.find(
//         daysFromCivil(selectedDate_.year, selectedDate_.month, selectedDate_.day));
//     if (found != dayInformation_.end()) info = &found->second;
//     if (!info) return;
//     if (calendarStore_) calendarStore_->saveWorkStatus(selectedDate_, WorkStatus::unspecified);
//     info->workStatus = WorkStatus::unspecified;
//     if (info->notes.empty() && info->tasks.empty() && info->events.empty() &&
//         info->taskCount == 0 && info->eventCount == 0)
//         dayInformation_.erase(found);
// }
// // ——— End of clearSelectedWorkStatus() ————————————————————————————————————————
// 
// 
// 
// void RCalHeaderFramework::loadDayInfo(Date date) {
//     const long long key = daysFromCivil(date.year, date.month, date.day);
//     DayInfo info;
//     if (calendarStore_) info = calendarStore_->loadDay(date);
//     else {
//         const auto found = dayInformation_.find(key);
//         if (found != dayInformation_.end()) info = found->second;
//         for (const auto& record : memoryEvents_) {
//             const auto& event = record.second;
//             if (eventOnDate(event, calendarDateString(date)) &&
//                 std::none_of(info.events.begin(), info.events.end(),
//                     [&](const Event& existing) { return existing.id == event.id; }))
//                 info.events.push_back(event);
//         }
//         std::stable_sort(info.events.begin(), info.events.end(), [](const Event& a, const Event& b) {
//             if (a.startDate != b.startDate) return a.startDate < b.startDate;
//             if (a.time.empty() != b.time.empty()) return !a.time.empty();
//             return a.time < b.time;
//         });
//     }
//     if (info.workStatus == WorkStatus::unspecified && info.notes.empty() && info.tasks.empty() &&
//         info.events.empty() && info.taskCount == 0 && info.eventCount == 0) {
//         dayInformation_.erase(key);
//     } else {
//         dayInformation_[key] = std::move(info);
//         const auto& loaded = dayInformation_[key];
//         if (sameDate(date, selectedDate_) && itemCount(loaded)) {
//             const std::size_t count = selectedItemType_ == DayItemType::note ? loaded.notes.size()
//                 : selectedItemType_ == DayItemType::task ? loaded.tasks.size() : loaded.events.size();
//             if (selectedItemIndex_ >= count) selectFlatIndex(loaded, 0);
//         }
//     }
// }
// 
// 
// void RCalHeaderFramework::discardEmptyDayInfo(Date date) {
//     const auto found = dayInformation_.find(daysFromCivil(date.year, date.month, date.day));
//     if (found == dayInformation_.end()) return;
//     const DayInfo& info = found->second;
//     if (info.workStatus == WorkStatus::unspecified && info.notes.empty() && info.tasks.empty() &&
//         info.events.empty() && info.taskCount == 0 && info.eventCount == 0)
//         dayInformation_.erase(found);
// }
// 
// std::size_t RCalHeaderFramework::itemCount(const DayInfo& info) const noexcept {
//     return info.notes.size() + info.tasks.size() + info.events.size();
// }
// 
// std::size_t RCalHeaderFramework::selectedFlatIndex(const DayInfo& info) const noexcept {
//     if (selectedItemType_ == DayItemType::note) return selectedItemIndex_;
//     if (selectedItemType_ == DayItemType::task) return info.notes.size() + selectedItemIndex_;
//     return info.notes.size() + info.tasks.size() + selectedItemIndex_;
// }
// 
// void RCalHeaderFramework::selectFlatIndex(const DayInfo& info, std::size_t index) noexcept {
//     if (index < info.notes.size()) {
//         selectedItemType_ = DayItemType::note;
//         selectedItemIndex_ = index;
//     } else if (index < info.notes.size() + info.tasks.size()) {
//         selectedItemType_ = DayItemType::task;
//         selectedItemIndex_ = index - info.notes.size();
//     } else {
//         selectedItemType_ = DayItemType::event;
//         selectedItemIndex_ = index - info.notes.size() - info.tasks.size();
//     }
// }
// 
// 
// void RCalHeaderFramework::loadTimelineDayInformation() {
//     if (calendarStore_) dayInformation_.clear();
//     for (const TimelineDay& day : timeline_) loadDayInfo(day.date);
// }
// 
// 
void RCalHeaderFramework::rebuildTimeline() {
    validateDate(options_.startDate);
    if (options_.dayCount == 0) throw std::runtime_error("dayCount must be at least 1");
    if (options_.bufferDays == 0) throw std::runtime_error("bufferDays must be at least 1");
    if (options_.rebuildThresholdDays >= options_.bufferDays)
        throw std::runtime_error("rebuildThresholdDays must be smaller than bufferDays");
    if (options_.cellWidth < 5) throw std::runtime_error("cellWidth must be at least 5");
    timeline_.clear();
    timeline_.reserve(options_.dayCount);
    const long long first = daysFromCivil(options_.startDate.year, options_.startDate.month,
                                          options_.startDate.day);
    for (std::size_t offset = 0; offset < options_.dayCount; ++offset) {
        const long long absolute = first + static_cast<long long>(offset);
        const Date date = civilFromDays(absolute);
        const unsigned weekday = isoWeekday(absolute);
        timeline_.push_back({date, ordinalDay(date), isoWeek(date, weekday), weekday,
                             holidayName(date, options_.holidayCountry)});
    }
    selectedDate_ = options_.startDate;
    selectedDayIndex_ = 0;
    firstVisibleDay_ = 0;
    visibleDayCount_ = 0;
    selectedItemType_ = DayItemType::note;
    selectedItemIndex_ = 0;
    viewportInitialized_ = false;
//     loadTimelineDayInformation();
}
// ——— End of rebuildTimeline() ————————————————————————————————————————————————



void RCalHeaderFramework::rebuildBufferedTimeline(Date selected, std::size_t visibleDays,
                                                  std::size_t selectedOffset) {
    validateDate(selected);
    selectedOffset = std::min(selectedOffset, visibleDays - 1);
    const std::size_t buffer = options_.bufferDays;
    const Date firstDate = addDays(selected,
        -static_cast<long long>(buffer + selectedOffset));
    const std::size_t count = buffer + visibleDays + buffer;

    timeline_.clear();
    timeline_.reserve(count);
    const long long first = daysFromCivil(firstDate.year, firstDate.month, firstDate.day);
    for (std::size_t offset = 0; offset < count; ++offset) {
        const Date date = civilFromDays(first + static_cast<long long>(offset));
        const unsigned weekday = isoWeekday(first + static_cast<long long>(offset));
        timeline_.push_back({date, ordinalDay(date), isoWeek(date, weekday), weekday,
                             holidayName(date, options_.holidayCountry)});
    }

    selectedDate_ = selected;
    firstVisibleDay_ = buffer;
    selectedDayIndex_ = buffer + selectedOffset;
    visibleDayCount_ = visibleDays;
    viewportInitialized_ = true;
//     loadTimelineDayInformation();
}
// ——— End of rebuildBufferedTimeline() ————————————————————————————————————————



void RCalHeaderFramework::ensureViewport(WINDOW* window, int left) {
    int windowHeight = 0;
    int windowWidth = 0;
    getmaxyx(window, windowHeight, windowWidth);
    (void)windowHeight;

    const std::array<std::string, 8> labels{{
        options_.yearLabel, options_.monthLabel, options_.weekLabel,
        options_.dayOfYearLabel, options_.dateLabel, options_.dayLabel,
        options_.timeInfoLabel, options_.detailsLabel
    }};
    std::size_t labelWidth = 1;
    for (const auto& label : labels) labelWidth = std::max(labelWidth, label.size());
    const int firstCell = left + static_cast<int>(labelWidth) + 1;
    const int available = std::max(0, windowWidth - firstCell);
    const std::size_t stride = options_.cellWidth + (options_.showGrid ? 1U : 0U);
    const std::size_t completeCells = static_cast<std::size_t>(available) +
        (options_.showGrid ? 1U : 0U);
    const std::size_t visibleDays = completeCells / stride;
    if (visibleDays == 0) {
        visibleDayCount_ = 0;
        return;
    }

    if (!viewportInitialized_ || visibleDays != visibleDayCount_) {
        rebuildBufferedTimeline(selectedDate_, visibleDays, visibleDays / 2);
    }
}
// ——— End of ensureViewport() —————————————————————————————————————————————————



void RCalHeaderFramework::moveSelection(int days)
{    
    if (days == 0) return;

    const Date moved = addDays(selectedDate_, days);
    validateDate(moved);
    selectedItemType_ = DayItemType::note;
    selectedItemIndex_ = 0;

    if (!viewportInitialized_ || visibleDayCount_ == 0){
        const long long candidate = static_cast<long long>(selectedDayIndex_) + days;
        if (candidate >= 0 && candidate < static_cast<long long>(timeline_.size())) {
            selectedDayIndex_ = static_cast<std::size_t>(candidate);
            selectedDate_ = moved;
        } else {
            options_.startDate = moved;
            rebuildTimeline();
        }
//         loadDayInfo(selectedDate_);
        return;
    }

    const long long candidate = static_cast<long long>(selectedDayIndex_) + days;
    if (candidate < 0 || candidate >= static_cast<long long>(timeline_.size())) {
        rebuildBufferedTimeline(moved, visibleDayCount_, visibleDayCount_ / 2);
        return;
    }

    selectedDate_ = moved;
    selectedDayIndex_ = static_cast<std::size_t>(candidate);
    if (selectedDayIndex_ < firstVisibleDay_) {
        firstVisibleDay_ = selectedDayIndex_;
    } else if (selectedDayIndex_ >= firstVisibleDay_ + visibleDayCount_) {
        firstVisibleDay_ = selectedDayIndex_ - visibleDayCount_ + 1;
    }

    const std::size_t rightEdge = firstVisibleDay_ + visibleDayCount_;
    const bool nearLeft = firstVisibleDay_ < options_.rebuildThresholdDays;
    const bool nearRight = timeline_.size() - std::min(rightEdge, timeline_.size()) <
                           options_.rebuildThresholdDays;
    if (nearLeft || nearRight) {
        const std::size_t selectedOffset = selectedDayIndex_ - firstVisibleDay_;
        rebuildBufferedTimeline(selectedDate_, visibleDayCount_, selectedOffset);
    }
//     loadDayInfo(selectedDate_);
}
// ——— End of moveSelection() ——————————————————————————————————————————————————



void RCalHeaderFramework::selectDate(Date date) {
    validateDate(date);
    selectedDate_ = date;
    selectedItemType_ = DayItemType::note;
    selectedItemIndex_ = 0;
    if (viewportInitialized_ && visibleDayCount_ > 0)
        rebuildBufferedTimeline(date, visibleDayCount_, visibleDayCount_ / 2);
    else {
        options_.startDate = date;
//         loadDayInfo(selectedDate_);
    }
}
// ——— End of selectDate() —————————————————————————————————————————————————————



InputResult RCalHeaderFramework::handleInput(int key, WINDOW* window) {
    if (!window) throw std::invalid_argument("input window must not be null");
    switch (key) {
// Disabled for the calendar-only design preview.
//         case KEY_UP: moveNoteSelection(-1); return InputResult::redraw;
//         case KEY_DOWN: moveNoteSelection(1); return InputResult::redraw;
//         case 'w': toggleSelectedWorkStatus(WorkStatus::workday); return InputResult::redraw;
//         case 't': toggleSelectedWorkStatus(WorkStatus::travelling); return InputResult::redraw;
//         case 'o':
//             toggleSelectedWorkStatus(WorkStatus::outOfOfficeWorking);
//             return InputResult::redraw;
//         case 'a':
//             toggleSelectedWorkStatus(WorkStatus::compensatoryLeave);
//             return InputResult::redraw;
//         case 'f': toggleSelectedWorkStatus(WorkStatus::vacation); return InputResult::redraw;
//         case ' ': clearSelectedWorkStatus(); return InputResult::redraw;
//         case 'x': toggleSelectedTask(); return InputResult::redraw;
//         case KEY_DC: return deleteSelectedItem() ? InputResult::redraw : InputResult::ignored;
        case 'q':
        case 'Q':
        case 27:
            return InputResult::quit;
        case KEY_LEFT:
            moveSelection(-1);
            return InputResult::redraw;
        case KEY_RIGHT:
            moveSelection(1);
            return InputResult::redraw;
        case KEY_PPAGE:
            moveSelection(-static_cast<int>(std::max<std::size_t>(1, visibleDayCount_)));
            return InputResult::redraw;
        case KEY_NPAGE:
            moveSelection(static_cast<int>(std::max<std::size_t>(1, visibleDayCount_)));
            return InputResult::redraw;
        case KEY_HOME:
        case 'n':
            selectDate(currentLocalDate());
            return InputResult::redraw;
        case KEY_RESIZE:
            viewportInitialized_ = false;
            return InputResult::redraw;
        default:
            return InputResult::ignored;
    }
}
// ——— End of handleInput() ————————————————————————————————————————————————————



int RCalHeaderFramework::noteRow(WINDOW* window, std::size_t noteIndex, int top) const {
    const int availableRows = std::max(0, getmaxy(window) - (top + 10) - 2);
    if (availableRows == 0) return -1;
    return top + 10 + static_cast<int>(std::min(noteIndex,
        static_cast<std::size_t>(availableRows - 1)));
}

int RCalHeaderFramework::render(WINDOW* window, int top, int left, bool /*addingNote*/) {
    if (!window) throw std::invalid_argument("render window must not be null");
    if (top < 0 || left < 0)
        throw std::invalid_argument("render position must not be negative");

    int windowHeight = 0;
    int windowWidth = 0;
    getmaxyx(window, windowHeight, windowWidth);
    if (top >= windowHeight || left >= windowWidth) return 0;
    ensureViewport(window, left);

    curs_set(0);
    if (has_colors()) { start_color(); use_default_colors(); }
    StyleRegistry styles;
    const unsigned width = options_.cellWidth;
    const std::array<std::string, 8> labels{{
        options_.yearLabel, options_.monthLabel, options_.weekLabel,
        options_.dayOfYearLabel, options_.dateLabel, options_.dayLabel,
        options_.timeInfoLabel, options_.detailsLabel
    }};
    std::size_t labelWidth = 1;
    for (const auto& label : labels) labelWidth = std::max(labelWidth, label.size());
    const int firstCell = left + static_cast<int>(labelWidth) + 1;

    for (int row = 0; row < 8; ++row) {
        put(window, top + row, left, labels[static_cast<std::size_t>(row)],
            styles.attributes(colors_.label));
        if (options_.showGrid)
            put(window, top + row, firstCell - 1, "|", styles.attributes(colors_.grid));
    }

    const unsigned stride = width + (options_.showGrid ? 1U : 0U);
    auto renderSpans = [&](int row, const std::vector<std::string>& values,
                           const ColorStyle& style) {
        std::size_t begin = 0;
        while (begin < values.size()) {
            std::size_t end = begin + 1;
            while (end < values.size() && values[end] == values[begin]) ++end;
            const unsigned spanWidth = static_cast<unsigned>((end - begin) * stride -
                                       (options_.showGrid ? 1U : 0U));
            const int x = firstCell + static_cast<int>(begin * stride);
            if (x >= windowWidth) break;

            // Center a group title in the visible part of the span. This keeps
            // long groups, such as a whole month, labelled even when their
            // natural centre is outside the terminal viewport.
            const int visibleEnd = std::min(windowWidth, x + static_cast<int>(spanWidth));
            const unsigned visibleWidth = static_cast<unsigned>(std::max(0, visibleEnd - x));
            put(window, top + row, x, centered(values[begin], visibleWidth),
                styles.attributes(style));
            if (options_.showGrid && x + static_cast<int>(spanWidth) < windowWidth)
                put(window, top + row, x + static_cast<int>(spanWidth), "|",
                    styles.attributes(colors_.grid));
            begin = end;
        }
    };

    std::vector<std::string> years;
    std::vector<std::string> months;
    std::vector<std::string> weeks;
    const std::size_t visibleEnd = std::min(timeline_.size(),
                                             firstVisibleDay_ + visibleDayCount_);
    years.reserve(visibleDayCount_);
    months.reserve(visibleDayCount_);
    weeks.reserve(visibleDayCount_);
    for (std::size_t i = firstVisibleDay_; i < visibleEnd; ++i) {
        const auto& day = timeline_[i];
        years.push_back(std::to_string(day.date.year));
        months.push_back(options_.monthNames.at(day.date.month - 1));
        weeks.push_back(std::to_string(day.isoWeek));
    }
    renderSpans(0, years, colors_.year);
    renderSpans(1, months, colors_.month);
    renderSpans(2, weeks, colors_.weekNumber);

    const Date today = currentLocalDate();
    for (std::size_t i = firstVisibleDay_; i < visibleEnd; ++i) {
        const auto& day = timeline_[i];
        const std::size_t visibleIndex = i - firstVisibleDay_;
        const int x = firstCell + static_cast<int>(visibleIndex * stride);
        if (x >= windowWidth) break;
        const bool isToday = sameDate(day.date, today);
        const bool isSelected = i == selectedDayIndex_;
        const bool isHoliday = !day.holidayName.empty();
        const ColorStyle& normalDayOfYearStyle = isHoliday ? colors_.holiday : colors_.dayOfYear;
        const ColorStyle& normalDateStyle = isHoliday ? colors_.holiday : colors_.date;
        const ColorStyle& normalDayStyle = isHoliday ? colors_.holiday
            : (day.isoWeekday >= 6 ? colors_.weekend : colors_.dayName);
        const ColorStyle& normalInfoStyle = isHoliday ? colors_.holiday : colors_.timeInfo;
        const ColorStyle currentDayOfYearStyle = isToday
            ? highlightedStyle(normalDayOfYearStyle, colors_.currentDay) : normalDayOfYearStyle;
        const ColorStyle currentDateStyle = isToday
            ? highlightedStyle(normalDateStyle, colors_.currentDay) : normalDateStyle;
        const ColorStyle currentDayStyle = isToday
            ? highlightedStyle(normalDayStyle, colors_.currentDay) : normalDayStyle;
        const ColorStyle currentInfoStyle = isToday
            ? highlightedStyle(normalInfoStyle, colors_.currentDay) : normalInfoStyle;
        const ColorStyle dayOfYearStyle = isSelected
            ? highlightedStyle(currentDayOfYearStyle, colors_.selectedDay)
            : currentDayOfYearStyle;
        const ColorStyle dateStyle = isSelected
            ? highlightedStyle(currentDateStyle, colors_.selectedDay) : currentDateStyle;
        const ColorStyle dayStyle = isSelected
            ? highlightedStyle(currentDayStyle, colors_.selectedDay) : currentDayStyle;
        const ColorStyle infoStyle = isSelected
            ? highlightedStyle(currentInfoStyle, colors_.selectedDay) : currentInfoStyle;
        put(window, top + 3, x, centered(std::to_string(day.dayOfYear), width), styles.attributes(dayOfYearStyle));
        put(window, top + 4, x, centered(std::to_string(day.date.day), width), styles.attributes(dateStyle));
        put(window, top + 5, x, centered(options_.dayNames.at(day.isoWeekday - 1), width), styles.attributes(dayStyle));
        put(window, top + 6, x, centered(infoIndicator(day.date), width), styles.attributes(infoStyle));
        if (options_.showGrid) {
            const int separator = x + static_cast<int>(width);
            for (int row = 3; row < 7; ++row)
                put(window, top + row, separator, "|", styles.attributes(colors_.grid));
        }
    }

    const std::string& selectedHoliday = selectedHolidayName();
    if (!selectedHoliday.empty()) {
        const std::string explanation = options_.holidayInfoPrefix.empty()
            ? selectedHoliday : options_.holidayInfoPrefix + ": " + selectedHoliday;
        put(window, top + 7, firstCell, explanation, styles.attributes(colors_.holiday));
    }
// Disabled for the calendar-only design preview.
//     const DayInfo* selectedInfo = dayInfo(selectedDate_);
//     const std::size_t noteCount = selectedInfo ? selectedInfo->notes.size() : 0;
//     const std::size_t taskCount = selectedInfo ? selectedInfo->tasks.size() : 0;
//     const std::size_t eventCount = selectedInfo ? selectedInfo->events.size() : 0;
//     if (windowHeight > top + 10) {
//         put(window, top + 9, left,
//             "Notes " + std::to_string(noteCount) + "  Tasks " + std::to_string(taskCount) +
//                 "  Events " + std::to_string(eventCount) +
//                 "  Up/Down: select  Enter: edit  Del: delete  x: complete  Insert: create  ?: help",
//             styles.attributes(colors_.label));
//         const int availableRows = std::max(0, windowHeight - (top + 10) - 2);
//         const std::size_t itemTotal = selectedInfo ? itemCount(*selectedInfo) : 0;
//         std::size_t firstItem = 0;
//         const std::size_t activeItem = addingNote ? itemTotal
//             : (selectedInfo ? selectedFlatIndex(*selectedInfo) : 0);
//         if (availableRows > 0 && activeItem >= static_cast<std::size_t>(availableRows))
//             firstItem = activeItem - static_cast<std::size_t>(availableRows) + 1;
//         const auto today = localDateString();
//         const auto now = localTimeString();
//         for (int row = 0; row < availableRows && firstItem + row < itemTotal; ++row) {
//             const std::size_t itemIndex = firstItem + static_cast<std::size_t>(row);
//             const bool overdue = itemIndex >= noteCount && itemIndex < noteCount + taskCount &&
//                 taskOverdue(selectedInfo->tasks[itemIndex - noteCount], today, now);
//             const ColorStyle& baseStyle = itemIndex < noteCount ? colors_.notes
//                 : itemIndex < noteCount + taskCount
//                     ? (overdue ? colors_.overdueTask : colors_.tasks) : colors_.events;
//             const ColorStyle itemStyle = itemIndex == selectedFlatIndex(*selectedInfo)
//                 ? colors_.selectedItem : baseStyle;
//             std::string text;
//             if (itemIndex < noteCount) {
//                 text = "Note: " + selectedInfo->notes[itemIndex].text;
//             } else if (itemIndex < noteCount + taskCount) {
//                 const Task& task = selectedInfo->tasks[itemIndex - noteCount];
//                 text = std::string(overdue ? "! " : "") +
//                     (task.completed ? "[x] Task: " : "[ ] Task: ") + task.text;
//                 if (!task.dueDate.empty())
//                     text += "  [due " + task.dueDate + (task.dueTime.empty() ? "" : " " + task.dueTime) + "]";
//                 if (overdue) text += " OVERDUE";
//             } else {
//                 const Event& event = selectedInfo->events[itemIndex - noteCount - taskCount];
//                 text = "Event: " + event.text + "  [start " +
//                     (event.startDate.empty() ? calendarDateString(selectedDate_) : event.startDate) +
//                     (event.time.empty() ? "" : " " + event.time) + " to " +
//                     event.endDate + (event.endTime.empty() ? "" : " " + event.endTime) + "]";
//             }
//             put(window, top + 10 + row, left,
//                 " " + std::to_string(itemIndex + 1) + ". " + text, styles.attributes(itemStyle));
//         }
//     }
    if (windowHeight > top + 8) {
        const std::string date = std::to_string(selectedDate_.year) + "-" +
            (selectedDate_.month < 10 ? "0" : "") + std::to_string(selectedDate_.month) + "-" +
            (selectedDate_.day < 10 ? "0" : "") + std::to_string(selectedDate_.day);
        const std::string status = " " + date + " | " + infoDescription(selectedDate_) + " ";
        const attr_t statusAttributes = styles.attributes(colors_.statusBar);
        wattrset(window, statusAttributes);
        mvwhline(window, windowHeight - 1, left, ' ', windowWidth - left);
        put(window, windowHeight - 1, left, status, statusAttributes);
    }
    wattrset(window, A_NORMAL);
    return std::min(8, windowHeight - top);
}
// ——— End of render() —————————————————————————————————————————————————————————



}
// End of namespace rcal ———————————————————————————————————————————————————————

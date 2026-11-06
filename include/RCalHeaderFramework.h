#pragma once

// #include "CalendarTypes.h" // Not available in the framework preview.

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct _win_st;
typedef struct _win_st WINDOW;

namespace rcal{

// Minimal date type for the standalone calendar framework.
struct Date {
    int year = 1970;
    unsigned month = 1;
    unsigned day = 1;
};

class CalendarStore;



struct ColorStyle{
    std::string foreground = "default";
    std::string background = "default";

    bool bold       = false;
    bool underline  = false;
    bool reverse    = false;
};

// Applies a highlight background and attributes while retaining the content foreground.
ColorStyle highlightedStyle(const ColorStyle& base, const ColorStyle& highlight);



struct HeaderColors{
    ColorStyle label;
    ColorStyle year;
    ColorStyle month;
    ColorStyle weekNumber;
    ColorStyle dayOfYear;
    ColorStyle date;
    ColorStyle dayName;
    ColorStyle weekend;
    ColorStyle holiday{"red", "default", true, false, false};
    ColorStyle currentDay{"black", "yellow", true, false, false};
    ColorStyle selectedDay{"default", "blue", false, false, false};
    ColorStyle selectedItem{"black", "cyan", true, false, false};
    ColorStyle timeInfo;
    ColorStyle notes{"white", "default"};
    ColorStyle tasks{"yellow", "default"};
    ColorStyle overdueTask{"red", "default", true, false, false};
    ColorStyle events{"cyan", "default"};
    ColorStyle grid;
    ColorStyle statusBar{"black", "white", false, false, false};
};



struct HeaderOptions{
    Date startDate;
    std::size_t dayCount = 31;
    std::size_t bufferDays = 90;
    std::size_t rebuildThresholdDays = 30;
    unsigned cellWidth = 5;
    bool showGrid = true;
    std::string holidayCountry = "NO";
    std::string dataFile;

    std::array<std::string, 12> monthNames{{
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    }};

    std::array<std::string, 7> dayNames{{
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    }};

    std::string yearLabel       = "Year";
    std::string monthLabel      = "Month";
    std::string weekLabel       = "Week";
    std::string dayOfYearLabel  = "DOY";
    std::string dateLabel       = "Date";
    std::string dayLabel        = "Day";
    std::string timeInfoLabel   = "Info";
    std::string detailsLabel    = "Detail";
    std::string holidayInfoPrefix = "Helligdag";
};



struct TimelineDay{
    Date date;
    unsigned dayOfYear  = 1;
    unsigned isoWeek    = 1;
    unsigned isoWeekday = 1;
    std::string holidayName;
};



enum class InputResult{
    ignored,
    redraw,
    quit
};

enum class DayItemType {
    note,
    task,
    event
};



class RCalHeaderFramework{
public:
    RCalHeaderFramework();
    explicit RCalHeaderFramework(HeaderOptions options);

    static RCalHeaderFramework fromConfigFile(const std::string& path);
    void loadConfigFile(const std::string& path);

    const HeaderOptions& options() const noexcept;
    const HeaderColors& colors() const noexcept;
    const std::vector<TimelineDay>& timeline() const noexcept;
    const Date& selectedDate() const noexcept;
    const std::string& selectedHolidayName() const noexcept;
    std::size_t firstVisibleDay() const noexcept;
    std::size_t visibleDayCount() const noexcept;
// Disabled for the calendar-only design preview.
//     const DayInfo* dayInfo(Date date) const noexcept;
//     const std::string* selectedNote() const noexcept;
//     std::size_t selectedNoteIndex() const noexcept;
//     DayItemType selectedItemType() const noexcept;
//     std::size_t selectedItemIndex() const noexcept;
//     std::size_t selectedItemFlatIndex() const noexcept;
//     std::string selectedItemText() const;
    std::string infoIndicator(Date date) const;
    std::string infoDescription(Date date) const;

    void setOptions(HeaderOptions options);
    void setColors(HeaderColors colors);
// Disabled for the calendar-only design preview.
//     void setCalendarStore(std::shared_ptr<CalendarStore> store);
//     void setDayInfo(Date date, DayInfo info);
//     void addNote(Date date, std::string note);
//     void addTask(Date date, std::string task);
//     void addTask(Date date, Task task);
//     void updateSelectedTask(Task task);
//     const Task* selectedTask() const noexcept;
//     void addEvent(Date date, std::string event);
//     void addEvent(Date date, Event event);
//     void updateSelectedEvent(Event event);
//     const Event* selectedEvent() const noexcept;
//     void updateSelectedNote(std::string note);
//     void updateSelectedItem(std::string text);
//     bool deleteSelectedItem();
//     void moveNoteSelection(int rows);
//     void toggleSelectedTask();
//     bool removeDayInfo(Date date);
//     void toggleSelectedWorkStatus(WorkStatus status);
//     void clearSelectedWorkStatus();
    void rebuildTimeline();
    void moveSelection(int days);
    void selectDate(Date date);
    InputResult handleInput(int key, WINDOW* window);

    // The caller must initialize curses with initscr() before rendering.
    // Returns the number of terminal rows written.
    int render(WINDOW* window, int top = 0, int left = 0, bool addingNote = false);
    int noteRow(WINDOW* window, std::size_t noteIndex, int top = 0) const;

private:
    void ensureViewport(WINDOW* window, int left);
    void rebuildBufferedTimeline(Date selected, std::size_t visibleDays,
                                 std::size_t selectedOffset);
// Disabled for the calendar-only design preview.
//     void loadDayInfo(Date date);
//     void refreshEventViews(const EventId& id, const Event* event);
//     void loadTimelineDayInformation();
//     void discardEmptyDayInfo(Date date);
//     std::size_t itemCount(const DayInfo& info) const noexcept;
//     std::size_t selectedFlatIndex(const DayInfo& info) const noexcept;
//     void selectFlatIndex(const DayInfo& info, std::size_t index) noexcept;
// 
    HeaderOptions options_;
    HeaderColors colors_;
    std::vector<TimelineDay> timeline_;
// Disabled for the calendar-only design preview.
//     std::map<long long, DayInfo> dayInformation_;
//     std::shared_ptr<CalendarStore> calendarStore_;
//     std::map<EventId, Event> memoryEvents_;
//     unsigned long long nextEventId_ = 0;
    Date selectedDate_;
    std::size_t selectedDayIndex_ = 0;
    std::size_t firstVisibleDay_ = 0;
    std::size_t visibleDayCount_ = 0;
    DayItemType selectedItemType_ = DayItemType::note;
    std::size_t selectedItemIndex_ = 0;
    bool viewportInitialized_ = false;
};

} // namespace rcal

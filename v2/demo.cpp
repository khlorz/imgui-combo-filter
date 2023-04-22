#include "demo.h"
#include "imgui-combo-filter.h"

#include <vector>
#include <array>
#include <span>
#include <string>

struct DemoPair
{
    const char* str;
    int num;
};

static const char* item_getter1(const std::vector<std::string>& items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].c_str();
    }
    return "N/A";
}

static const char* item_getter2(std::span<const std::string> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].c_str();
    }
    return "...";
}

static const char* item_getter3(std::span<const DemoPair> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index].str;
    }
    return "";
}

static const char* item_getter4(std::span<const char* const> items, int index) {
    if (index >= 0 && index < (int)items.size()) {
        return items[index];
    }
    return "";
}

// Fuzzy search algorith by @r-lyeh with some adjustments of my own
static bool fuzzy_score(const char* str1, const char* str2, int& score)
{
    score = 0;
    if (*str2 == '\0')
        return *str1 == '\0';

    int consecutive = 0;
    int maxerrors = 0;

    while (*str1 && *str2) {
        int is_leading = (*str1 & 64) && !(str1[1] & 64);
        if ((*str1 & ~32) == (*str2 & ~32)) {
            int had_separator = (str1[-1] <= 32);
            int x = had_separator || is_leading ? 10 : consecutive * 5;
            consecutive = 1;
            score += x;
            ++str2;
        }
        else {
            int x = -1, y = is_leading * -3;
            consecutive = 0;
            score += x;
            maxerrors += y;
        }
        ++str1;
    }

    score += (maxerrors < -9 ? -9 : maxerrors);
    return *str2 == '\0';
};

// Creating a user-defined callback instead of using the default callback for ComboAutoSelectSearchCallback
// The callback is recommended to be templated so you can use it anywhere, and be usable on any type (if possible)
template<typename T>
int autoselect_search(const ImGui::ComboAutoSelectSearchCallbackData<T>& cbd)
{
    // The callback maker is responsible for dealing with empty strings and other possible rare string variants for this particular callback
    // For this demo, an empty string is a not-found/failure so we return -1... But you are free to deal with this howevery you like
    if (cbd.SearchString[0] == '\0')
        return -1;

    int items_count = static_cast<int>(std::size(cbd.Items));
    int best = -1;
    int i = 0;
    int score;
    int scoremax;
    for (; i < items_count; ++i) {
        const char* word_i = cbd.ItemGetter(cbd.Items, i);
        if (fuzzy_score(word_i, cbd.SearchString, score)) {
            scoremax = score;
            best = i;
            break;
        }
    }
    for (; i < items_count; ++i) {
        const char* word_i = cbd.ItemGetter(cbd.Items, i);
        if (fuzzy_score(word_i, cbd.SearchString, score)) {
            if (score > scoremax) {
                scoremax = score;
                best = i;
            }
        }
    }
    return best;
};

// However, if you only really need a certain type then...
// But do not forget the constness of the container type
int autoselect_search_vector(const ImGui::ComboAutoSelectSearchCallbackData<const std::vector<std::string>&>& cbd)
{
    if (cbd.SearchString[0] == '\0')
        return -1;

    int items_count = static_cast<int>(cbd.Items.size());
    int best = -1;
    int i = 0;
    int score;
    int scoremax;
    for (; i < items_count; ++i) {
        const char* word_i = cbd.Items[i].c_str(); // We do not use the ItemGetter here since the type is already known
        if (fuzzy_score(word_i, cbd.SearchString, score)) {
            scoremax = score;
            best = i;
            break;
        }
    }
    for (; i < items_count; ++i) {
        const char* word_i = cbd.Items[i].c_str(); // We can just use the operator [] without any out of bounds access
        if (fuzzy_score(word_i, cbd.SearchString, score)) {
            if (score > scoremax) {
                scoremax = score;
                best = i;
            }
        }
    }
    return best;
}

// Create a user-defined callback instead of using the default callback for ComboFilterSearchCallback
// The same as ComboAutoSelectSearchCallback, it's recommended to make the callback a template
template<typename T>
void filter_search(const ImGui::ComboFilterSearchCallbackData<T>& cbd)
{
    // Contrary to ComboAutoSelectSearchCallback, this callback will not be called on an empty search string
    // So this is not needed here... Only shown for demo purposes
    //if (cbd.SearchString[0] == '\0') {
    //    /* Do something here */
    //}

    const int items_count = static_cast<int>(std::size(cbd.Items));
    for (int i = 0; i < items_count; ++i) {
        int score = 0;
        if (fuzzy_score(cbd.ItemGetter(cbd.Items, i), cbd.SearchString, score))
            cbd.FilterResults->emplace_back(i, score); // You can just use emplace_back. Parameters are the index and score respectively
    }

    // Sorting is optional
    // It depends on you or your data if you want to sort the list a different a way or not at all
    ImGui::SortFilterResultsDescending(*cbd.FilterResults);
}

namespace ImGui
{

void ShowComboAutoSelectDemo(bool* p_open)
{
    if (p_open && !(*p_open))
        return;

    if (ImGui::Begin("ComboAutoSelect Demo", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::vector<std::string> items1{ "instruction", "Chemistry", "Beating Around the Bush", "Instantaneous Combustion", "Level 999999", "nasal problems", "On cloud nine", "break the iceberg", "lacircificane" };
        static int selected_item1 = -1;
        if (ImGui::ComboAutoSelect("std::vector combo", selected_item1, items1, item_getter1, autoselect_search_vector, ImGuiComboFlags_HeightSmall)) {
            /* Selection made */
        }

        static std::array<std::string, 11> items2{ "arm86", "chicanery", "A quick brown fox", "jumps over the lazy dog", "Budaphest Hotel", "Grand", "The", "1998204", "1-0-1-0-0-1xx", "end", "Alphabet" };
        static int selected_item2 = -1;
        if (ImGui::ComboAutoSelect("std::array combo", selected_item2, items2, item_getter2, autoselect_search, ImGuiComboFlags_HeightLarge)) {
            /* Selection made */
        }

        // In-case you need to create a ComboData before calling the widget, which can be cumbersome (intentional because it's really only for internal usage)
        static bool create_auto_select = false;
        static DemoPair items3[]{ {"bury the dark", 1}, {"bury the", 8273}, {"bury", 0}, {"bur", 777}, {"dig", 943}, {"dig your", 20553}, {"dig your motivation", 6174}, {"max concentration", 5897}, {"crazyyyy", 31811} };
        if (create_auto_select) {
            static int selected_item3 = -1;
            if (ImGui::ComboAutoSelect("c-array combo", selected_item3, items3, item_getter3)) {
                /* Selection made */
            }
        }
        else if (ImGui::Button("Create ComboAutoSelect widget")) {
            create_auto_select = true;
            auto combo_data = ImGui::Internal::AddComboData<ImGui::ComboAutoSelectData>("c-array combo"); // NOTE: The hash id is dependent on the window the combo will be in, otherwise it will be unaccessible and leak would occur
            combo_data->InitialValues.Index = 3;
            combo_data->InitialValues.Preview = items3[3].str;
        }

        static bool remove_empty_combo = false;
        if (!remove_empty_combo) {
            static std::vector<std::string> items4;
            static int selected_item4 = -1;
            if (ImGui::ComboAutoSelect("empty combo", selected_item4, items4, item_getter2, autoselect_search)) {
                /* Selection made */
            }

            if (ImGui::Button("Remove ComboAutoSelect widget above")) {
                remove_empty_combo = true;
                ImGui::ClearComboData("empty combo");
                // As long as you know the window the combo is in and the combo label, you can clear it from anywhere
                // This can be handy if you need to free memory when the widget will not be in use for sometime
                // The same goes for AddComboData and GetComboData
                // ImGui::ClearComboData("ComboAutoSelect Demo", "empty combo");
            }
        }
    }
	ImGui::End();
}

void ShowComboFilterDemo(bool* p_open)
{
    if (p_open && !(*p_open))
        return;

    static bool remove_third_combofilter = false;
    if (ImGui::Begin("ComboFilter Demo", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* items1[]{ "tolerationism", "tediferous", "tach", "thermokinematics", "interamnian", "pistolography", "discerptible", "subulate", "sententious", "salt" };
        static int selected_item1 = -1;
        if (ImGui::ComboFilter("c-array 1", selected_item1, items1, item_getter4, filter_search, ImGuiComboFlags_HeightLargest)) {
            /* Selection made */
        }

        static std::string items2[]{ "level", "leveler", "MacroCallback.cpp", "Miskatonic university", "MockAI.h", "MockGameplayTasks.h", "MovieSceneColorTrack.cpp", "r.maxfps", "r.maxsteadyfps", "reboot", "rescale", "reset", "resource", "restart", "retrocomputer", "retrograd", "return", "slomo 10" };
        static int selected_item2 = -1;
        if (ImGui::ComboFilter("c-array 2", selected_item2, items2, item_getter2, ImGuiComboFlags_HeightSmall)) {
            /* Selection made */
        }

        if (!remove_third_combofilter) {
            static std::array<std::string, 5> items3{"array element 0", "Accelerando", "Soprano", "Crescendo", "Arpeggio"};
            static int selected_item3 = -1;
            if (ImGui::ComboFilter("std::array no-arrow", selected_item3, items3, item_getter2, ImGuiComboFlags_NoArrowButton)) {
                /* Selection made */
            }
        }

        static std::vector<std::string> items7{ "exaggerate", "error", "impress", "mechanism", "Electromagnetic Impulse", "uninterestingly amazing", "Sweet and salty flavor", "zebra stripes", "strike", "sweat", "axe", "OK" };
        static int selected_item4 = -1;
        if (ImGui::ComboFilter("std::vector no-preview", selected_item4, items7, item_getter1, filter_search , ImGuiComboFlags_NoPreview)) {
            /* Selection made */
        }

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImVec2 next_window_pos(window->Pos.x, window->Pos.y + window->Size.y + 5.0f);
        ImGui::SetNextWindowPos(next_window_pos, ImGuiCond_Always);
    }
    ImGui::End();


    if (ImGui::Begin("Another Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!remove_third_combofilter) {
            // Clearing a combo data remotely
            if (ImGui::Button("Remove the third ComboFilter widget")) {
                ImGui::ClearComboData("ComboFilter Demo", "std::array no-arrow");
                remove_third_combofilter = true;
            }
        }
        else {
            if (ImGui::Button("Restore the third ComboFilter widget")) {
                remove_third_combofilter = false;
            }
        }
    }
    ImGui::End();
}

}

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
// Template fuzzy search function so it can accept any container
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

// Create a user-defined callback instead of using the default callback for AutoSelectSearchCallback
template<typename T>
int autoselect_search(const ImGui::ComboAutoSelectSearchCallbackData<T>& cbd)
{
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

// Create a user-defined callback instead of using the default callback for FilterSearchCallback
template<typename T>
void filter_search(const ImGui::ComboFilterSearchCallbackData<T>& cbd)
{
    const int items_count = static_cast<int>(std::size(cbd.Items));
    for (int i = 0; i < items_count; ++i) {
        int score = 0;
        if (fuzzy_score(cbd.ItemGetter(cbd.Items, i), cbd.SearchString, score))
            cbd.FilterResults->emplace_back(i, score);
    }

    ImGui::SortFilterResultsDescending(*cbd.FilterResults);
}

void ComboAutoSelectDemo()
{
    if (ImGui::Begin("ComboAutoSelect Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::vector<std::string> items1{ "instruction", "Chemistry", "Beating Around the Bush", "Instantaneous Combustion", "Level 999999", "nasal problems", "On cloud nine", "break the iceberg", "lacircificane" };
        static int selected_item1 = -1;
        if (ImGui::ComboAutoSelect("std::vector combo", selected_item1, items1, item_getter1, ImGuiComboFlags_HeightSmall)) {
            /* Selection made */
        }

        static std::array<std::string, 11> items2{ "arm86", "chicanery", "A quick brown fox", "jumps over the lazy dog", "Budaphest Hotel", "Grand", "The", "1998204", "1-0-1-0-0-1xx", "end", "Alphabet" };
        static int selected_item2 = -1;
        if (ImGui::ComboAutoSelect("std::array combo", selected_item2, items2, item_getter2, autoselect_search, ImGuiComboFlags_HeightLarge)) {
            /* Selection made */
        }

        // In-case you need to make some customization, which can be cumbersome (sadly)
        static bool customize_autoselect = false;
        static DemoPair items3[]{ {"bury the dark", 1}, {"bury the", 8273}, {"bury", 0}, {"bur", 777}, {"dig", 943}, {"dig your", 20553}, {"dig your motivation", 6174}, {"max concentration", 5897}, {"crazyyyy", 31811} };
        if (customize_autoselect) {
            static int selected_item3 = -1;
            if (ImGui::ComboAutoSelect("c-array combo", selected_item3, items3, item_getter3)) {
                /* Selection made */
            }
        }
        else if (ImGui::Button("Create ComboAutoSelect widget")) {
            customize_autoselect = true;
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

void ComboFilterDemo()
{
    static bool remove_third_combofilter = false;
    if (ImGui::Begin("ComboFilter Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
            if (ImGui::ComboFilter("std::array arrow-only", selected_item3, items3, item_getter2, ImGuiComboFlags_NoArrowButton)) {
                /* Selection made */
            }
        }

        static std::vector<std::string> items7{ "exaggerate", "error", "impress", "mechanism", "Electromagnetic Impulse", "uninterestingly amazing", "Sweet and salty flavor", "zebra stripes", "strike", "sweat", "axe", "OK" };
        static int selected_item4 = -1;
        if (ImGui::ComboFilter("std::vector no-preview", selected_item4, items7, item_getter2, filter_search, ImGuiComboFlags_NoPreview)) {
            /* Selection made */
        }
    }
    ImGui::End();

    if (ImGui::Begin("Another Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Clearing a combo data remotely
        if (ImGui::Button("Remove the third ComboFilter widget")) {
            ImGui::ClearComboData("ComboFilter Demo", "std::array arrow-only");
            remove_third_combofilter = true;
        }
    }
    ImGui::End();
}

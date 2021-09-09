// progi.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <string>
#include <iostream>
#include <map>
#include <chrono>
#include <regex>
#include <list>
#include <iomanip>
#include <fstream>


std::string config_file_location = "dummyconf.conf";
unsigned int time_before_possible_car_ms = 2000;
unsigned int time_until_winner_ms = 4000;
std::regex plate_line_regex("^[\\s]+-[\\s]+[\\S]+[\\s]+confidence:\\s[\\S]+", std::regex_constants::ECMAScript | std::regex_constants::icase);
double percent_sum_threshold = 200;
unsigned int sliding_interval_ms = 60000;
unsigned int top_n_display = 5;

// CHECKED
void parseArguments(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" || arg == "-c") {
            config_file_location = argv[i + 1];
        }
    }
}

// CHECKED
void readConfig() {
    std::ifstream infile(config_file_location);
    std::string key;
    std::string value;
    std::string eql_symbol;
    while (infile >> key >> eql_symbol >> value)
    {
        if (key == "time_before_possible_car_ms") time_before_possible_car_ms = std::stoi(value);
        else if (key == "time_until_winner_ms") time_until_winner_ms = std::stoi(value);
        else if (key == "percent_sum_threshold") percent_sum_threshold = std::stod(value);
        else if (key == "sliding_interval_ms") sliding_interval_ms = std::stoi(value);
        else if (key == "top_n_display") top_n_display = std::stoi(value);
    }
}

// CHECKED
void removeOutsideSlidingInterval(std::map <std::string, std::tuple<double, std::time_t, std::time_t>>& plates, std::multimap <std::time_t, std::pair <std::string, std::tuple<double, std::time_t, std::time_t>* >>& plates_time_index) {
    auto bound = plates_time_index.upper_bound(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - sliding_interval_ms);
    for (auto it = plates_time_index.begin(); it != bound; ++it) {
        std::string plate_to_remove = (*it).second.first;
        plates.erase(plate_to_remove);
    }
    plates_time_index.erase(plates_time_index.begin(), bound);
    return;
}


// CHECKED
bool isCarDetected(std::string plate_line, std::map <std::string, std::tuple<double, std::time_t, std::time_t>>& plates) {

    size_t plate_first = 6;
    size_t plate_last = plate_line.find("\t", plate_first + 1) - 1 < plate_line.find(" ", plate_first + 1) - 1 ? plate_line.find("\t", plate_first + 1) - 1 : plate_line.find(" ", plate_first + 1) - 1;
    //size_t confidence_first = plate_line.find("confidence: ", plate_last + 1) + 12;
    //size_t confidence_last = plate_line.find_last_of(".0123456789");
    //double confidence = std::stod(plate_line.substr(confidence_first, confidence_last - confidence_first + 1));
    std::string plate = plate_line.substr(plate_first, plate_last - plate_first + 1);
    if (std::get<0>(plates[plate]) > percent_sum_threshold) {
        return true;
    }
    return false;
}

// CHECKED
void addPlateToCollection(std::string plate_line, std::map <std::string, std::tuple<double, std::time_t, std::time_t>>& plates, std::multimap <std::time_t, std::pair <std::string, std::tuple<double, std::time_t, std::time_t>* >>& plates_time_index) {
    size_t plate_first = 6;
    size_t plate_last = plate_line.find("\t", plate_first + 1) - 1 < plate_line.find(" ", plate_first + 1) - 1 ? plate_line.find("\t", plate_first + 1) - 1 : plate_line.find(" ", plate_first + 1) - 1;
    size_t confidence_first = plate_line.find("confidence: ", plate_last + 1) + 12;
    size_t confidence_last = plate_line.find_last_of(".0123456789");
    double confidence = std::stod(plate_line.substr(confidence_first, confidence_last - confidence_first + 1));
    std::string plate = plate_line.substr(plate_first, plate_last - plate_first + 1);
    std::time_t time_now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (plates.find(plate) != plates.end()) { // meglévő bejegyzés
        std::get<0>(plates[plate]) += confidence;
        std::get<2>(plates[plate]) = time_now;
        for (auto it = plates_time_index.begin(); it != plates_time_index.end(); it++) {
            if ((*it).second.second == &(plates[plate])) {
                plates_time_index.erase(it);
                break;
            }
        }
    }
    else {
        plates[plate] = std::tuple<double, std::time_t, std::time_t>(confidence, time_now, time_now);
    }

    plates_time_index.insert(std::make_pair(time_now, std::make_pair(plate, &plates[plate])));
}

// CHECKED
bool compare_winners(const std::pair<std::string, double>& first, const std::pair<std::string, double>& second) {
    return (first.second > second.second);
}


// CHECKED
void carPassingBy(std::map <std::string, std::tuple<double, std::time_t, std::time_t>>& plates, std::multimap <std::time_t, std::pair <std::string, std::tuple<double, std::time_t, std::time_t>* >>& plates_time_index) {
    //stdset - ből most - timebeforepossiblecar - nál régebbi bejegyzések törlése
    auto bound = plates_time_index.upper_bound(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - time_before_possible_car_ms);
    for (auto it = plates_time_index.begin(); it != bound; ++it) {
        std::string plate_to_remove = (*it).second.first;
        plates.erase(plate_to_remove);
    }
    plates_time_index.erase(plates_time_index.begin(), bound);
    //további sorok beolvasása timeuntilwinner ideig
    std::time_t when_will_be_winner = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + time_until_winner_ms;
    std::string line;
    while (std::getline(std::cin, line) && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() < when_will_be_winner) {
        if (std::regex_search(line, plate_line_regex)) {
            addPlateToCollection(line, plates, plates_time_index);
        }
    }
    //győztes sorrend felállítása
    std::list<std::pair<std::string, double>> winners;
    for (auto it = plates.begin(); it != plates.end(); ++it)
    {
        winners.push_back(std::pair<std::string, double>(it->first, std::get<0>(it->second)));
    }
    winners.sort(compare_winners);

    //győztes sorrend stdout - ra írása
    std::list<std::pair<std::string, double>>::iterator it = winners.begin();
    std::cout << "----------" << std::endl;
    for (unsigned i = 0; i < top_n_display && it != winners.end(); ++it, ++i) {
        std::cout << "[PLATE][" << it->first << "][";
        std::cout << std::fixed << std::setprecision(2) << it->second << "]" << std::endl;
    }
    std::cout << "----------" << std::endl;

    //kollekciók kitisztítása
    plates.clear();
    plates_time_index.clear();
    return;
}




// CHECKED
int main(int argc, char** argv)
{
    parseArguments(argc, argv);
    readConfig();
    std::string line;
    std::map <std::string, std::tuple<double, std::time_t, std::time_t>> plates; //rendszám, biztosságösszeg, első megjelenés, utolsó megjelenés
    std::multimap <std::time_t, std::pair <std::string, std::tuple<double, std::time_t, std::time_t>* >> plates_time_index;

    while (std::getline(std::cin, line))
    {
        removeOutsideSlidingInterval(plates, plates_time_index);
        if (std::regex_search(line, plate_line_regex)) {
            addPlateToCollection(line, plates, plates_time_index);

            if (isCarDetected(line, plates)) {
                carPassingBy(plates, plates_time_index);
            }
        }
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

#include "Pattern.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <fstream>

namespace wako::seq
{

    std::vector<int> Pattern::activeAtStep(int step) const
    {
        std::vector<int> result;
        result.reserve(MAX_PADS);
        for (int p = 0; p < MAX_PADS; ++p)
            if (grid[p][step])
                result.push_back(p);
        return result;
    }

    Pattern &Pattern::clearAll()
    {
        for (auto &row : grid)
            row.fill(false);
        currentStep = 0;
        return *this;
    }

    Pattern &Pattern::clearPad(int pad)
    {
        if (pad >= 0 && pad < MAX_PADS)
            grid[pad].fill(false);
        return *this;
    }

    int Pattern::advance()
    {
        int played = currentStep;
        currentStep = (currentStep + 1) % patternLength;
        return played;
    }

    bool Pattern::saveToFile(const std::string &path) const
    {
        nlohmann::json j;
        j["bpm"] = bpm;
        j["length"] = patternLength;

        auto jgrid = nlohmann::json::array();
        for (int p = 0; p < MAX_PADS; ++p)
        {
            auto row = nlohmann::json::array();
            for (int s = 0; s < MAX_STEPS; ++s)
                row.push_back(grid[p][s]);
            jgrid.push_back(row);
        }
        j["grid"] = jgrid;

        std::ofstream f(path);
        if (!f.is_open())
            return false;
        f << j.dump(2);
        return true;
    }

    std::optional<Pattern> Pattern::loadFromFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f.is_open())
            return std::nullopt;

        nlohmann::json j;
        try
        {
            f >> j;
        }
        catch (...)
        {
            return std::nullopt;
        }

        Pattern pat;
        pat.setBpm(j.value("bpm", 120));
        pat.setLength(j.value("length", 16));

        if (j.contains("grid") && j["grid"].is_array())
        {
            for (int p = 0; p < MAX_PADS && p < (int)j["grid"].size(); ++p)
            {
                const auto &row = j["grid"][p];
                for (int s = 0; s < MAX_STEPS && s < (int)row.size(); ++s)
                    pat.grid[p][s] = row[s].get<bool>();
            }
        }
        return pat;
    }

}

/*
 * 2021 Tarpeeksi Hyvae Soft
 *
 * Software: VCS
 *
 */

#include <cmath>
#include "filter/filters/input_gate/filter_input_gate.h"
#include "filter/filters/input_gate/gui/filtergui_input_gate.h"

filtergui_input_gate_c::filtergui_input_gate_c(filter_c *const filter)
{
    {
        auto *const width = new filtergui_spinbox_s;

        width->get_value = [=]{return filter->parameter(filter_input_gate_c::PARAM_WIDTH);};
        width->set_value = [=](const double value){filter->set_parameter(filter_input_gate_c::PARAM_WIDTH, value);};
        width->minValue = 0;
        width->maxValue = MAX_CAPTURE_WIDTH;

        this->guiFields.push_back({"Width", {width}});
    }

    {
        auto *const height = new filtergui_spinbox_s;

        height->get_value = [=]{return filter->parameter(filter_input_gate_c::PARAM_HEIGHT);};
        height->set_value = [=](const double value){filter->set_parameter(filter_input_gate_c::PARAM_HEIGHT, value);};
        height->minValue = 0;
        height->maxValue = MAX_CAPTURE_HEIGHT;

        this->guiFields.push_back({"Height", {height}});
    }

    return;
}

/*
 * 2021 Tarpeeksi Hyvae Soft
 *
 * Software: VCS
 *
 */

#ifndef VCS_FILTER_FILTERS_MEDIAN_FILTER_MEDIAN_H
#define VCS_FILTER_FILTERS_MEDIAN_FILTER_MEDIAN_H

#include "filter/filter.h"
#include "filter/filters/median/gui/filtergui_median.h"

class filter_median_c : public filter_c
{
public:
    enum parameter_offset_e { PARAM_KERNEL_SIZE = 0 };

    filter_median_c(const u8 *const initialParameterArray = nullptr) :
        filter_c(initialParameterArray,
                 {filter_c::make_parameter<u8, PARAM_KERNEL_SIZE>(3)})
    {
        this->guiDescription = new filtergui_median_c(this);
    }

    std::string uuid(void) const override { return "de60017c-afe5-4e5e-99ca-aca5756da0e8"; }
    std::string name(void) const override { return "Median"; }
    filter_type_e type(void) const override { return filter_type_e::median; }
    filter_category_e category(void) const override { return filter_category_e::reduce; }

    void apply(FILTER_FUNC_PARAMS) const override;

private:
};

#endif

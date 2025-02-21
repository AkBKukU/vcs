﻿/*
 * 2018, 2020 Tarpeeksi Hyvae Soft
 *
 * Software: VCS
 *
 */

/*! @file
 *
 * @brief
 * The filter subsystem interface.
 * 
 * The filter subsystem provides facilities for manipulating captured frames'
 * pixel data.
 * 
 * The building block of this subsystem is the image filter. They're subclasses of
 * abstract_filter_c that modify image pixels in some way: blurring, sharpening,
 * rotation, and so on.
 * 
 * Instances of image filters are organized into filter chains. A filter chain
 * consists of one or more filters, each of which will be applied in turn to an
 * input frame. A filter chain consisting of a blur filter, a sharpen filter, and
 * a rotation filter would first blur the frame, then sharpen it, and finally
 * rotate it.
 * 
 * Each filter chain is associated with an input condition and an output condition,
 * based on which the subsystem makes a choice about which of the chains to apply
 * to an input frame. A chain's input condition dictates the resolution the input
 * frame must be in to be acted on by the chain's filters, and the output condition
 * specifies the resolution the scaler subsystem must be outputting in for the
 * chain to be applied. The first filter chain whose input and output conditions
 * are fulfilled will be applied to the input frame.
 * 
 * For example, a filter chain with an input condition of 640 x 480 and an output
 * condition of 800 x 600 will be applied to an input frame if the frame's resolution
 * is 640 x 480 and the scaler subsystem is set to produce its output in 800 x 600
 * (i.e. it'll scale this frame to 800 x 600 at a later stage in VCS's capture
 * pipeline). If there's a second filter chain with these same conditions, only
 * the first chain will be applied and not the second.
 *
 * ## Usage
 *
 *   1. Call kf_initialize_filters() to initialize the filter subsystem. This is
 *      VCS's default startup behavior.
 *
 *   2. Use kf_create_filter_instance() to create instances of filters:
 *      @code
 *      // Create an instance of a blurring filter.
 *      auto *blur = kf_create_filter_instance<filter_blur_c>();
 * 
 *      // You can also use the non-templated instancer, which takes a UUID string
 *      // identifying the type of filter to create.
 *      auto *flip = kf_create_filter_instance("80a3ac29-fcec-4ae0-ad9e-bbd8667cc680");
 * 
 *      // "80a3ac29-fcec-4ae0-ad9e-bbd8667cc680" == filter_flip_c().uuid().
 *      @endcode
 * 
 *   3. Get and set filters' parameter values:
 *      @code
 *      auto *blur = kf_create_filter_instance<filter_blur_c>();
 * 
 *      // Increase the blurring radius.
 *      const double radius = blur->parameter(filter_blur_c::PARAM_KERNEL_SIZE);
 *      blur->set_parameter(filter_blur_c::PARAM_KERNEL_SIZE, (radius + 1));
 * 
 *      // Use a box kernel instead of the default Gaussian.
 *      blur->set_parameter(filter_blur_c::PARAM_TYPE, filter_blur_c::BLUR_BOX);
 * 
 *      // Note: Each filter also includes a GUI widget that provides the end-user graphical
 *      // controls for adjusting the filter parameters. VCS's display subsystem provides an
 *      // interactive filter graph that exposes these widgets to the user.
 *      @endcode
 *
 *   4. Combine filters into filter chains:
 *      @code
 *      // Create some filters.
 *      auto *blur = kf_create_filter_instance<filter_blur_c>();
 *      auto *flip = kf_create_filter_instance<filter_flip_c>();
 * 
 *      // Create the chain's input and output conditions. This chain will be applied to
 *      // frames of size 640 x 480 when the scaler subsystem's output size is 800 x 600.
 *      auto *in = kf_create_filter_instance<filter_input_gate_c>({{0, 640}, {1, 480}});
 *      auto *out = kf_create_filter_instance<filter_output_gate_c>({{0, 800}, {1, 600}});
 * 
 *      // Create the chain. Note that chains must begin with an input condition and end
 *      // with an output condition.
 *      std::vector<abstract_filter_c*> chain = {in, blur, flip, out};
 * 
 *      // Make the filter subsystem aware of the chain.
 *      kf_register_filter_chain(chain);
 * 
 *      // Note: VCS's display subsystem provides the end-user an interactive filter
 *      // graph for building filter chains via the GUI.
 *      @endcode
 * 
 *   5. Apply suitable filter chains to captured frames:
 *      @code
 *      kc_evNewCapturedFrame.listen([](const captured_frame_s &frame)
 *      {
 *          kf_apply_matching_filter_chain(frame.pixels.data(), frame.r);
 *      });
 *      @endcode
 * 
 *   6. (Optional) Apply filters individually:
 *      @code
 *      auto *blur = kf_create_filter_instance<filter_blur_c>();
 * 
 *      // Filter a dummy image of 1 x 2 resolution (BGRA/8888 format).
 *      uint8_t pixels[8];
 *      resolution_s resolution = {1, 2, 32};
 *      blur->apply(pixels, resolution);
 * 
 *      // When we no longer need the filter.
 *      kf_delete_filter_instance(blur);
 *      @endcode
 * 
 *   7. Call kf_release_filters() to release the filter subsystem. This is VCS's
 *      default exit behavior.
 * 
 * ## Implementing new filters
 * 
 * New filter types can be added by subclassing abstract_filter_c and filtergui_c
 * and declaring the new filter in kf_initialize_filters().
 * 
 * ### Sample implementation
 * 
 * Let's implement a filter, `class filter_filler_c`, that fills all the pixels
 * in the input image with a solid color.
 * 
 * A filter's implementation consists of two parts: the functional part (subclass of
 * abstract_filter_c), and the GUI part (subclass of filtergui_c). The functional
 * part provides methods with which the filter can be applied to pixel data, while
 * the GUI part provides the VCS end-user a GUI widget for manipulating the filter's
 * parameters.
 * 
 * #### Subclassing abstract_filter_c
 * 
 * In filter_filler.h:
 * @code
 * class filter_filler_c : public abstract_filter_c
 * {
 * public:
 *     CLONABLE_FILTER_TYPE(filter_filler_c)
 * 
 *     // The filter's user-customizable parameters. In this case, the fill color.
 *     enum { PARAM_RED,
 *            PARAM_GREEN,
 *            PARAM_BLUE };
 * 
 *     // The constructor that's called whenever a new instance of this filter is created.
 *     // Note: We set the default fill color to 150, 10, 200. Instances of the filter
 *     // will start with those values unless 'initialParamValues' specifies otherwise.
 *     filter_filler_c(const std::vector<std::pair<unsigned, double>> &initialParamValues = {}) :
 *         abstract_filter_c({{PARAM_RED, 150},
 *                            {PARAM_GREEN, 10},
 *                            {PARAM_BLUE, 200}},
 *                           initialParamValues)
 *     {
 *         this->guiDescription = new filtergui_filler_c(this);
 *     }
 * 
 *     // The function that applies the filter's processing to input pixels.
 *     void apply(u8 *const pixels, const resolution_s &r) override;
 * 
 *     // Metadata about the filter, uniquely identifying it from other filters.
 *     std::string uuid(void) const override { return "6f6b513d-359e-43c7-8de5-de29b1559d10"; }
 *     std::string name(void) const override { return "Filler"; }
 *     filter_category_e category(void) const override { return filter_category_e::reduce; }
 * };
 * @endcode
 * 
 * In filter_filler.cpp:
 * @code
 * void filter_filler_c::apply(u8 *const pixels, const resolution_s &r)
 * {
 *     // All filters should assert the validity of their input data.
 *     this->assert_input_validity(pixels, r);
 * 
 *     const uint8_t red   = this->parameter(PARAM_RED);
 *     const uint8_t green = this->parameter(PARAM_GREEN);
 *     const uint8_t blue  = this->parameter(PARAM_BLUE);
 *     const uint8_t alpha = 255;
 *     
 *     // We can use OpenCV if VCS is built with support for it.
 *     #if USE_OPENCV
 *         cv::Mat output = cv::Mat(r.h, r.w, CV_8UC4, pixels);
 *         output = cv::Scalar(blue, green, red, alpha);
 *     // Fallback for when VCS is built without OpenCV support.
 *     #else
 *         for (unsigned y = 0; y < r.h; y++)
 *         {
 *             for (unsigned x = 0; x < r.w; x++)
 *             {
 *                 const unsigned pixelIdx = ((x + y * r.w) * 4);
 *     
 *                 pixels[pixelIdx + 0] = blue;
 *                 pixels[pixelIdx + 1] = green;
 *                 pixels[pixelIdx + 2] = red;
 *                 pixels[pixelIdx + 3] = alpha;
 *             }
 *         }
 *     #endif
 * 
 *     return;
 * }
 * @endcode
 * 
 * #### Subclassing filtergui_c
 * 
 * In filtergui_filter_filler.h:
 * @code
 * class filtergui_filler_c : public filtergui_c
 * {
 * public:
 *     filtergui_filler_c(abstract_filter_c *const filter);
 * };
 * @endcode
 * 
 * In filtergui_filter_filler.cpp:
 * @code
 * // Construct a GUI framework-independent representation of the filter's GUI widget.
 * // VCS's display subsystem will create the actual GUI widget from this representation
 * // (using e.g. Qt).
 * //
 * // The 'filter' parameter is a reference to the filter instance whose parameters
 * // this widget controls.
 * filtergui_filler_c::filtergui_filler_c(abstract_filter_c *const filter)
 * {
 *     // A GUI element that lets the user specify a value from 0 to 255 for the red channel.
 *     // The set_value() function gets called when the user modifies the element's value.
 *     auto *const red = new filtergui_spinbox_s;
 *     red->get_value = [=]{return filter->parameter(filter_filler_c::PARAM_RED);};
 *     red->set_value = [=](const double value){filter->set_parameter(filter_filler_c::PARAM_RED, value);};
 *     red->minValue = 0;
 *     red->maxValue = 255;
 *     
 *     // Same as above but for the green channel.
 *     auto *const green = new filtergui_spinbox_s;
 *     green->get_value = [=]{return filter->parameter(filter_filler_c::PARAM_BIT_COUNT_GREEN);};
 *     green->set_value = [=](const double value){filter->set_parameter(filter_filler_c::PARAM_GREEN, value);};
 *     green->minValue = 0;
 *     green->maxValue = 255;
 *     
 *     // For the blue channel.
 *     auto *const blue = new filtergui_spinbox_s;
 *     blue->get_value = [=]{return filter->parameter(filter_filler_c::PARAM_BLUE);};
 *     blue->set_value = [=](const double value){filter->set_parameter(filter_filler_c::PARAM_BLUE, value);};
 *     blue->minValue = 0;
 *     blue->maxValue = 255;
 *     
 *     // Insert the elements into the filter widget onto a row labeled "RGB".
 *     this->guiFields.push_back({"RGB", {red, green, blue}});
 * 
 *     return;
 * }
 * @endcode
 * 
 * The new filter is now implemented. We just need to declare it in
 * kf_initialize_filters():
 * 
 * @code
 * void kf_initialize_filters(void)
 * {
 *     KNOWN_FILTER_TYPES =
 *     {
 *         new filter_blur_c(),
 *         new filter_delta_histogram_c(),
 *         // ...
 *         new filter_output_gate_c(),
 *         new filter_filler_c() // <- Our filter.
 *     };
 * }
 * @endcode
 * 
 * With that done, we can start using the filter:
 * 
 * @code
 * // Create an instance of the filter using the templated kf_create_filter_instance().
 * auto *filler = kf_create_filter_instance<filter_filler_c>();
 * 
 * // Or: Create an instance of the filter using its UUID with kf_create_filter_instance().
 * // auto *filler = kf_create_filter_instance("6f6b513d-359e-43c7-8de5-de29b1559d10").
 * 
 * filler->set_parameters({{filter_filler_c::PARAM_RED, 255},
 *                         {filter_filler_c::PARAM_GREEN, 0},
 *                         {filter_filler_c::PARAM_BLUE,  0}});
 * 
 * filler->apply(pixels, resolution);
 * @endcode
 * 
 * The new filter will also be included automatically as a selectable filter type
 * in the display subsystem's GUI filter graph.
 * 
 * @warning
 * Once established, you should never change a filter's UUID. It acts as a filter
 * type identifier when VCS's filter graphs are saved to -- and loaded from -- disk.
 */

#ifndef VCS_FILTER_FILTER_H
#define VCS_FILTER_FILTER_H

#include <unordered_map>
#include <functional>
#include <cstring>
#include "common/memory/heap_mem.h"
#include "filter/filtergui.h"
#include "display/display.h"
#include "common/globals.h"

/*!
 * @brief
 * Enumerates the functional categories into which filters can be divided.
 *
 * These categories exist e.g. for the benefit of the VCS GUI, allowing a more
 * structured listing of filters in menus.
 */
enum class filter_category_e
{
    /*!
     * Filters that reduce the image's fidelity. For example: blur, decimate.
     */
    reduce,

    /*!
     * Filters that enhance the image's fidelity. For example: sharpen, denoise.
     */
    enhance,

    /*!
     * Filters that modify the image's geometry. For example: rotate, crop.
     */
    distort,

    /*!
     * Filters that provide information about the image. For example: frame rate
     * estimate, noise histogram.
     */
    meta,

    /*!
     * Special case, not for use by filters. Used as a control in filter chains.
     */
    input_condition,

    /*!
     * Special case, not for use by filters. Used as a control in filter chains.
     */
    output_condition,
};

/*!
 * Initializes the filter subsystem, allocating its memory buffers etc.
 * 
 * @warning
 * This function must be called prior to any others in the filter subsystem.
 * 
 * @see
 * kf_release_filters()
 */
void kf_initialize_filters(void);

/*!
 * Releases the filter subsystem, including deallocating any of its memory
 * buffers and filter instances.
 * 
 * @warning
 * Between calling this function and kf_initialize_filters(), no other filter
 * subsystem function should be called. The pointers to any filter instances
 * created with kf_create_filter_instance() will be invalidated by this call.
 */
void kf_release_filters(void);

/*!
 * Adds @p newChain to the filter subsystem's list of known filter chains.
 * This makes the chain available for use by kf_apply_matching_filter_chain().
 * 
 * The chain's filter instances must be created using kf_create_filter_instance().
 * 
 * A filter chain must begin with a filter of type filter_category_e::input_condition
 * and end with a filter of type filter_category_e::output_condition.
 * 
 * @code
 * // Create some filters for a chain.
 * auto *inputGate = kf_create_filter_instance<filter_input_gate_c>();
 * auto *blur = kf_create_filter_instance<filter_blur_c>();
 * auto *flip = kf_create_filter_instance<filter_flip_c>();
 * auto *outputGate = kf_create_filter_instance<filter_output_gate_c>();
 * 
 * // Create the filter chain. 
 * std::vector<abstract_filter_c*> chain = {inputGate, blur, flip, outputGate};
 * 
 * // Make the chain available to the filter subsystem.
 * kf_register_filter_chain(chain);
 * @endcode
 *
 * @see
 * kf_unregister_all_filter_chains(), kf_apply_matching_filter_chain(), kf_create_filter_instance()
 */
void kf_register_filter_chain(std::vector<abstract_filter_c*> newChain);

/*!
 * Clears the filter subsystem's list of registered filter chains.
 * 
 * @warning
 * The chains' filter instances won't be deallocated and their memory will continue
 * to be managed by the filter subsystem. You need to call kf_delete_filter_instance()
 * if you want to deallocate them.
 *
 * @see
 * kf_register_filter_chain()
 */
void kf_unregister_all_filter_chains(void);

/*!
 * Applies to the @p pixels of an image of resolution @p r the first filter chain
 * registered with kf_register_filter_chain() whose input condition matches @p r
 * and whose output condition matches the scaler subsystem's current output
 * resolution. If there's no chain that matches these conditions, no chain will
 * be applied.
 * 
 * If the filter subsystem is disabled, or if there are no registered filter
 * chains, calling this function has no effect.
 * 
 * @note
 * @p pixels is expected to be in BGRA/8888 format, each pixel being 4 bytes in
 * B,G,R,A order.
 *
 * @see
 * kf_register_filter_chain(), ks_output_resolution(), kf_set_filtering_enabled()
 */
void kf_apply_matching_filter_chain(u8 *const pixels, const resolution_s &r);

/*!
 * Returns a list of the filter types that're available via this subsystem
 * interface.
 * 
 * @code
 * // Print the names of the available filter types.
 * for (const auto *filterType: kf_available_filter_types())
 * {
 *     std::cout << filterType->name() << std::endl;
 * }
 * @endcode
 * 
 * @see
 * kf_is_known_filter_uuid()
 */
const std::vector<const abstract_filter_c*>& kf_available_filter_types(void);

/*!
 * Creates a new instance of a filter, whose type is identified with a UUID by
 * @p filterTypeUuid and whose initial parameters values are given by @p initialParams.
 *
 * Returns a pointer to the created instance, or @a nullptr on error. The caller
 * can use the pointers obtained from this function to create filter chains.
 * 
 * @note
 * The returned pointer's memory is managed by the filter subsystem; the caller
 * shouldn't deallocate it. Its release can be requested via
 * kf_delete_filter_instance().
 *  
 * @code
 * // Create an instance of a particular filter, using a UUID to identify the
 * // filter's type (in this case, filter_blur_c().uuid()).
 * auto *blur = kf_create_filter_instance("a5426f2e-b060-48a9-adf8-1646a2d3bd41");
 * 
 * // Create an instance of a particular filter, using a template to identify
 * // the filter's type.
 * auto *blur2 = kf_create_filter_instance<filter_blur_c>();
 * @endcode
 *
 * @see
 * kf_register_filter_chain(), kf_delete_filter_instance()
 */
abstract_filter_c* kf_create_filter_instance(const std::string &filterTypeUuid,
                                             const std::vector<std::pair<unsigned, double>> &initialParamValues = {});

//!@cond
template <class T>
abstract_filter_c* kf_create_filter_instance(const std::vector<std::pair<unsigned, double>> &initialParamValues = {})
{
    return kf_create_filter_instance(T().uuid(), initialParamValues);
}
//!@endcond

/*!
 * Deallocates a filter instance created by kf_create_filter_instance().
 *
 * @warning
 * The caller must first unregister any filter chains that're using this filter.
 *
 * @see
 * kf_release_filters(), kf_unregister_all_filter_chains()
 */
void kf_delete_filter_instance(const abstract_filter_c *const filter);

/*!
 * Returns true if the given filter type UUID identifies a filter type available
 * via this subsystem interface; false otherwise.
 * 
 * @see
 * kf_available_filter_types(), kf_create_filter_instance()
 */
bool kf_is_known_filter_uuid(const std::string &filterTypeUuid);

/*!
 * Enable or disable the filter subsystem.
 *
 * This state has an effect on kf_apply_matching_filter_chain() such that the function
 * will do nothing if the filter subsystem is disabled. Other functionality of
 * the interface is not affected.
 *
 * @see
 * kf_is_filtering_enabled()
 */
void kf_set_filtering_enabled(const bool enabled);

/*!
 * Returns true if the filter subsystem is currently enabled; false otherwise.
 *
 * @see
 * kf_set_filtering_enabled()
 */
bool kf_is_filtering_enabled(void);

#endif

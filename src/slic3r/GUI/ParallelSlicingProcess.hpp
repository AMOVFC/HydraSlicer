#ifndef slic3r_GUI_ParallelSlicingProcess_hpp_
#define slic3r_GUI_ParallelSlicingProcess_hpp_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/thread.hpp>
#include <wx/event.h>

#include "libslic3r/PrintBase.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

namespace Slic3r {

class PresetBundle;

namespace GUI {

class PartPlate;

// Status of a single plate's slicing job.
struct PlateSlicingStatus {
    enum State { Pending, Running, Finished, Error, Cancelled };
    State              state = Pending;
    int                plate_index = -1;
    int                progress_percent = 0;  // 0-100
    std::string        message;
    std::exception_ptr exception;
};

// Event posted to the UI thread when all parallel slicing is complete.
class ParallelSlicingCompletedEvent : public wxEvent
{
public:
    ParallelSlicingCompletedEvent(wxEventType eventType, int winid,
                                  std::vector<PlateSlicingStatus> results)
        : wxEvent(winid, eventType), m_results(std::move(results)) {}

    wxEvent* Clone() const override { return new ParallelSlicingCompletedEvent(*this); }

    const std::vector<PlateSlicingStatus>& results() const { return m_results; }
    bool all_succeeded() const;
    bool any_error() const;
    // Returns the first error, or empty string.
    std::string first_error_message() const;

private:
    std::vector<PlateSlicingStatus> m_results;
};

wxDECLARE_EVENT(EVT_PARALLEL_SLICING_COMPLETED, ParallelSlicingCompletedEvent);

// Event posted periodically to the UI thread with aggregated progress.
class ParallelSlicingProgressEvent : public wxEvent
{
public:
    ParallelSlicingProgressEvent(wxEventType eventType, int winid,
                                 int overall_percent, const std::string& message)
        : wxEvent(winid, eventType), m_percent(overall_percent), m_message(message) {}

    wxEvent* Clone() const override { return new ParallelSlicingProgressEvent(*this); }

    int percent() const { return m_percent; }
    const std::string& message() const { return m_message; }

private:
    int         m_percent;
    std::string m_message;
};

wxDECLARE_EVENT(EVT_PARALLEL_SLICING_PROGRESS, ParallelSlicingProgressEvent);

// Manages parallel slicing of multiple build plates.
//
// Each plate already owns its own Print* and GCodeResult*, so they can be
// processed concurrently. The coordinator applies configs on the UI thread,
// then dispatches worker threads that call Print::process() + export_gcode().
//
// Usage:
//   1. Call prepare() on the UI thread to apply configs to all plates.
//   2. Call start() to spawn worker threads.
//   3. Listen for EVT_PARALLEL_SLICING_PROGRESS and EVT_PARALLEL_SLICING_COMPLETED.
//   4. Call cancel() to request cancellation (waits for workers to finish).
class ParallelSlicingProcess
{
public:
    ParallelSlicingProcess();
    ~ParallelSlicingProcess();

    // Set the wxEvtHandler that will receive progress/completion events.
    void set_event_handler(wxEvtHandler* handler) { m_event_handler = handler; }

    // Set the thumbnail rendering callback (called on UI thread).
    void set_thumbnail_cb(ThumbnailsGeneratorCallback cb) { m_thumbnail_cb = std::move(cb); }

    // Prepare all plates for slicing. Must be called on the UI thread.
    // Applies the model and config to each plate's Print object.
    // Returns true if all plates are ready, false if any validation failed.
    struct PrepareResult {
        bool success = true;
        std::string error_message;
        int  error_plate_index = -1;
    };
    PrepareResult prepare(const std::vector<PartPlate*>& plates,
                          const Model& model,
                          const DynamicPrintConfig& global_config,
                          PresetBundle& preset_bundle);

    // Start parallel slicing of all prepared plates.
    // Returns false if already running.
    bool start();

    // Request cancellation of all running jobs. Non-blocking.
    void cancel();

    // Block until all workers finish (after cancel or normal completion).
    void wait();

    // Is the parallel slicing currently running?
    bool running() const { return m_running.load(); }

    // Get per-plate status (thread-safe snapshot).
    std::vector<PlateSlicingStatus> get_status() const;

private:
    // Per-plate work item.
    struct PlateJob {
        PartPlate*            plate = nullptr;
        Print*                print = nullptr;
        GCodeProcessorResult* gcode_result = nullptr;
        std::string           temp_gcode_path;
        bool                  is_bbl_printer = false;
    };

    void worker_thread(int job_index);
    void post_progress();
    ThumbnailsList render_thumbnails_on_ui(const ThumbnailsParams& params);

    wxEvtHandler*              m_event_handler = nullptr;
    ThumbnailsGeneratorCallback m_thumbnail_cb;

    std::vector<PlateJob>       m_jobs;
    mutable std::mutex          m_status_mutex;
    std::vector<PlateSlicingStatus> m_status;

    std::vector<boost::thread>  m_workers;
    boost::thread               m_monitor;
    std::atomic<bool>           m_running{false};
    std::atomic<bool>           m_cancel_requested{false};
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_ParallelSlicingProcess_hpp_

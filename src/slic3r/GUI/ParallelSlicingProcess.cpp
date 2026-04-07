#include "ParallelSlicingProcess.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "PartPlate.hpp"

#include <wx/app.h>

#include "libslic3r/Print.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Thread.hpp"
#include "libslic3r/GCode/PostProcessor.hpp"

#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_PARALLEL_SLICING_COMPLETED, ParallelSlicingCompletedEvent);
wxDEFINE_EVENT(EVT_PARALLEL_SLICING_PROGRESS, ParallelSlicingProgressEvent);

bool ParallelSlicingCompletedEvent::all_succeeded() const
{
    for (const auto& r : m_results)
        if (r.state != PlateSlicingStatus::Finished)
            return false;
    return true;
}

bool ParallelSlicingCompletedEvent::any_error() const
{
    for (const auto& r : m_results)
        if (r.state == PlateSlicingStatus::Error)
            return true;
    return false;
}

std::string ParallelSlicingCompletedEvent::first_error_message() const
{
    for (const auto& r : m_results) {
        if (r.state == PlateSlicingStatus::Error) {
            if (r.exception) {
                try { std::rethrow_exception(r.exception); }
                catch (const std::exception& e) { return e.what(); }
                catch (...) { return "Unknown error"; }
            }
            return r.message.empty() ? "Unknown error" : r.message;
        }
    }
    return {};
}

ParallelSlicingProcess::ParallelSlicingProcess() = default;

ParallelSlicingProcess::~ParallelSlicingProcess()
{
    cancel();
    wait();
}

ParallelSlicingProcess::PrepareResult
ParallelSlicingProcess::prepare(const std::vector<PartPlate*>& plates,
                                const Model& model,
                                const DynamicPrintConfig& global_config,
                                PresetBundle& preset_bundle)
{
    PrepareResult result;
    m_jobs.clear();
    m_status.clear();

    for (PartPlate* plate : plates) {
        if (!plate || !plate->has_printable_instances())
            continue;

        Print* print = plate->fff_print();
        if (!print)
            continue;

        // Apply config to this plate's Print object.
        DynamicPrintConfig plate_config;
        if (plate->has_preset_override()) {
            plate_config = plate->build_full_config(preset_bundle);
        } else {
            plate_config = global_config;
        }

        // Apply filament maps for multi-extruder setups.
        if (preset_bundle.get_printer_extruder_count() > 1) {
            std::vector<int> f_maps = plate->get_real_filament_maps(preset_bundle.project_config);
            auto* filament_map_opt = plate_config.option<ConfigOptionInts>("filament_map", true);
            if (filament_map_opt)
                filament_map_opt->values = f_maps;
        }

        Print::ApplyStatus apply_status = print->apply(model, std::move(plate_config));
        (void)apply_status; // Any status is fine — process() handles incremental updates.
        print->is_BBL_printer() = preset_bundle.is_bbl_vendor();

        // Validate.
        StringObjectException warning;
        StringObjectException error = print->validate(&warning);
        if (!error.string.empty()) {
            result.success = false;
            result.error_message = error.string;
            result.error_plate_index = plate->get_index();
            BOOST_LOG_TRIVIAL(error) << "ParallelSlicing: plate " << plate->get_index()
                                     << " validation failed: " << error.string;
            return result;
        }

        PlateJob job;
        job.plate = plate;
        job.print = print;
        job.gcode_result = plate->get_slice_result();
        job.temp_gcode_path = plate->get_tmp_gcode_path();
        job.is_bbl_printer = preset_bundle.is_bbl_vendor();
        m_jobs.push_back(std::move(job));

        PlateSlicingStatus status;
        status.plate_index = plate->get_index();
        status.state = PlateSlicingStatus::Pending;
        m_status.push_back(std::move(status));
    }

    if (m_jobs.empty()) {
        result.success = false;
        result.error_message = "No printable plates to slice.";
    }

    return result;
}

bool ParallelSlicingProcess::start()
{
    if (m_running.load())
        return false;

    if (m_jobs.empty())
        return false;

    m_running.store(true);
    m_cancel_requested.store(false);
    m_workers.clear();
    m_workers.reserve(m_jobs.size());

    BOOST_LOG_TRIVIAL(info) << "ParallelSlicing: starting " << m_jobs.size() << " plate jobs in parallel";

    for (size_t i = 0; i < m_jobs.size(); ++i) {
        m_workers.emplace_back([this, i]() { this->worker_thread(static_cast<int>(i)); });
    }

    // Spawn a monitor thread that waits for all workers and posts the completion event.
    m_monitor = boost::thread([this]() {
        set_current_thread_name("par_slice_mon");
        for (auto& w : m_workers) {
            if (w.joinable())
                w.join();
        }
        m_running.store(false);

        if (m_event_handler) {
            std::vector<PlateSlicingStatus> final_status;
            {
                std::lock_guard<std::mutex> lk(m_status_mutex);
                final_status = m_status;
            }
            auto* evt = new ParallelSlicingCompletedEvent(
                EVT_PARALLEL_SLICING_COMPLETED, 0, std::move(final_status));
            wxQueueEvent(m_event_handler, evt);
        }

        BOOST_LOG_TRIVIAL(info) << "ParallelSlicing: all jobs complete";
    });

    return true;
}

void ParallelSlicingProcess::cancel()
{
    m_cancel_requested.store(true);
    // Cancel each Print to interrupt process().
    for (auto& job : m_jobs) {
        if (job.print)
            job.print->cancel();
    }
}

void ParallelSlicingProcess::wait()
{
    // Join the monitor thread, which itself joins all workers.
    if (m_monitor.joinable())
        m_monitor.join();
    m_workers.clear();
}

std::vector<PlateSlicingStatus> ParallelSlicingProcess::get_status() const
{
    std::lock_guard<std::mutex> lk(m_status_mutex);
    return m_status;
}

void ParallelSlicingProcess::worker_thread(int job_index)
{
    set_current_thread_name(("par_slice_" + std::to_string(job_index)).c_str());

    auto& job = m_jobs[job_index];
    const int plate_idx = job.plate->get_index();

    BOOST_LOG_TRIVIAL(info) << "ParallelSlicing: worker " << job_index
                            << " starting plate " << plate_idx;

    // Set per-plate status callback.
    job.print->set_status_callback([this, job_index, plate_idx](const PrintBase::SlicingStatus& status) {
        {
            std::lock_guard<std::mutex> lk(m_status_mutex);
            m_status[job_index].progress_percent = status.percent;
            m_status[job_index].message = status.text;
        }
        post_progress();
    });

    // Update status to Running.
    {
        std::lock_guard<std::mutex> lk(m_status_mutex);
        m_status[job_index].state = PlateSlicingStatus::Running;
    }
    post_progress();

    try {
        if (m_cancel_requested.load())
            throw CanceledException();

        // Reset gcode result before slicing.
        job.gcode_result->reset();

        // Run the slicing pipeline.
        job.print->process();

        // Update filament maps if auto-assigned.
        {
            std::vector<int> f_maps = job.print->get_filament_maps();
            if (!f_maps.empty())
                job.plate->set_filament_maps(f_maps);
        }

        if (m_cancel_requested.load())
            throw CanceledException();

        // Export G-code.
        job.print->export_gcode(job.temp_gcode_path, job.gcode_result,
            [this](const ThumbnailsParams& params) {
                return this->render_thumbnails_on_ui(params);
            });

        if (job.is_bbl_printer)
            run_post_process_scripts(job.temp_gcode_path, false, "File",
                                     job.temp_gcode_path, job.print->full_print_config());

        job.print->finalize();

        // Mark finished.
        {
            std::lock_guard<std::mutex> lk(m_status_mutex);
            m_status[job_index].state = PlateSlicingStatus::Finished;
            m_status[job_index].progress_percent = 100;
            m_status[job_index].message = "Complete";
        }

        // Update plate's slice validity.
        job.plate->update_slice_result_valid_state(true);

        BOOST_LOG_TRIVIAL(info) << "ParallelSlicing: plate " << plate_idx << " finished successfully";

    } catch (const CanceledException&) {
        std::lock_guard<std::mutex> lk(m_status_mutex);
        m_status[job_index].state = PlateSlicingStatus::Cancelled;
        m_status[job_index].message = "Cancelled";
        BOOST_LOG_TRIVIAL(info) << "ParallelSlicing: plate " << plate_idx << " cancelled";

    } catch (...) {
        std::lock_guard<std::mutex> lk(m_status_mutex);
        m_status[job_index].state = PlateSlicingStatus::Error;
        m_status[job_index].exception = std::current_exception();
        try { std::rethrow_exception(std::current_exception()); }
        catch (const std::exception& e) {
            m_status[job_index].message = e.what();
            BOOST_LOG_TRIVIAL(error) << "ParallelSlicing: plate " << plate_idx
                                     << " error: " << e.what();
        }
        catch (...) {
            m_status[job_index].message = "Unknown error";
            BOOST_LOG_TRIVIAL(error) << "ParallelSlicing: plate " << plate_idx
                                     << " unknown error";
        }
    }

    job.print->restart();
    post_progress();
}

void ParallelSlicingProcess::post_progress()
{
    if (!m_event_handler)
        return;

    int total_percent = 0;
    std::string msg;
    int count = 0;
    int running_count = 0;
    {
        std::lock_guard<std::mutex> lk(m_status_mutex);
        count = static_cast<int>(m_status.size());
        for (const auto& s : m_status) {
            total_percent += s.progress_percent;
            if (s.state == PlateSlicingStatus::Running) {
                running_count++;
            }
        }
    }

    int overall = count > 0 ? total_percent / count : 0;
    msg = "Slicing " + std::to_string(count) + " plates (" + std::to_string(overall) + "%)";

    auto* evt = new ParallelSlicingProgressEvent(
        EVT_PARALLEL_SLICING_PROGRESS, 0, overall, msg);
    wxQueueEvent(m_event_handler, evt);
}

ThumbnailsList ParallelSlicingProcess::render_thumbnails_on_ui(const ThumbnailsParams& params)
{
    ThumbnailsList thumbnails;
    if (!m_thumbnail_cb)
        return thumbnails;

    // Synchronously execute on the UI thread.
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

    wxGetApp().mainframe->m_plater->CallAfter([this, &params, &thumbnails, &mtx, &cv, &done]() {
        thumbnails = m_thumbnail_cb(params);
        {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
        }
        cv.notify_all();
    });

    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&done]{ return done; });
    return thumbnails;
}

} // namespace GUI
} // namespace Slic3r

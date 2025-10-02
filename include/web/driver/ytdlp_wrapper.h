/**
 * \file
 * \brief  Class to wrap yt-dlp funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

#include <Python.h>

#include <string>

#include "model/application_error.h"
#include "model/song.h"
#include "nlohmann/json_fwd.hpp"
#include "util/logger.h"
#include "web/base/stream_fetcher.h"

namespace driver {

/**
 * @brief Class to embed python to use yt-dlp and extract streaming information from the given URL
 */
class YtDlpWrapper : public web::StreamFetcher {
  //! Variable name used in python snippet that indicates if fetched successfully an stream
  static constexpr std::string_view kStreamFound = "result";

  //! Variable name used in python snippet that contains title information
  static constexpr std::string_view kAudioTitle = "title";

  //! Variable name used in python snippet that contains duration information
  static constexpr std::string_view kAudioDuration = "duration";

  //! Variable name used in python snippet that contains streaming information
  static constexpr std::string_view kStreamInfo = "streams";

  //! Python snippet used as wrapper to execute yt_dlp
  static constexpr std::string_view kExtractInfo = R"(
import json
import yt_dlp

URL = '###'

class DummyLogger:
    def debug(self, msg):
        pass
    def info(self, msg):
        pass
    def warning(self, msg):
        pass
    def error(self, msg):
        pass

ydl_opts = {
    'logger': DummyLogger(),
}

result = False

with yt_dlp.YoutubeDL(ydl_opts) as ydl:
    info = ydl.extract_info(URL, download=False)

    parsed = json.loads(json.dumps(ydl.sanitize_info(info)))

    filtered = list(filter(lambda x: (x['resolution'] == 'audio only'), parsed["formats"]))
    filtered.sort(key=lambda x: x["quality"], reverse=True)

    if len(filtered):
      result = True
      title = parsed["title"]
      duration = parsed["duration"]
      streams = json.dumps(filtered))";

 public:
  /**
   * @brief Construct a new YtDlpWrapper object
   */
  YtDlpWrapper() = default;

  /**
   * @brief Destroy the YtDlpWrapper object
   */
  virtual ~YtDlpWrapper() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Initialize internal structures for stream fetcher
   */
  void Init() override;

  /**
   * @brief Finish and clean up all internal structures from stream fetcher
   */
  void Finish() override;

  /**
   * @brief Extract streaming information from the given URL
   * @param song Song with a streaming URL, fetching operation will get the rest of the info (out)
   * @return Error code from operation
   */
  error::Code ExtractInfo(model::Song &song) override;

  /* ******************************************************************************************** */
  //! Internal methods
 private:
  /**
   * @brief Fill streaming information inside Song structure with content from parsed JSON
   * @param entry JSON parsed entry
   * @param duration Song duration (in seconds)
   * @param song Song information
   */
  void FillStreamInfo(const nlohmann::json &entry, uint32_t duration, model::Song &song);

  /**
   * @brief A utility struct for embedding Python interpreter in C++ application.
   * This class provides encapsulation of the Python C API for safer and more convenient
   * usage. It handles initialization, cleanup, code execution and variable retrieval.
   */
  class PythonWrapper {
   public:
    //! Default constructor/destructor
    PythonWrapper() = default;
    ~PythonWrapper() = default;

    //! Initialize embedded python and create main module
    void Init() {
      // NOTE: both Initialize and Finalize must be performed in the same thread, otherwise
      // segmentation fault may occur
      Py_Initialize();
      main_module_ = PyImport_AddModule("__main__");
      dict_ = PyModule_GetDict(main_module_);
    }

    //! Reset module and finalize python
    void Finish() { Py_Finalize(); }

    //! Execute code snippet and print any errors
    bool Run(const std::string &snippet) {
      processed_ = false;

      if (PyObject *result = PyRun_String(snippet.c_str(), Py_file_input, dict_, dict_); !result) {
        if (PyErr_Occurred()) {
          // Fetch error info
          PyObject *type, *value, *traceback;
          PyErr_Fetch(&type, &value, &traceback);
          PyErr_NormalizeException(&type, &value, &traceback);

          // Convert exception value to string
          PyObject *exception_str = PyObject_Str(value);
          std::string_view error_message = PyUnicode_AsUTF8(exception_str);

          ERROR("Python code snippet has throwed an exception=",
                !error_message.empty() ? error_message : "<unknown>");

          // Cleanup
          Py_XDECREF(exception_str);
          Py_XDECREF(type);
          Py_XDECREF(value);
          Py_XDECREF(traceback);
        }
      } else {
        processed_ = true;
      }

      return processed_;
    }

    //! Get value as bool from given variable
    bool GetBool(const std::string_view &variable) {
      if (!processed_) return "";

      if (PyObject *raw = PyDict_GetItemString(dict_, variable.data()); raw && PyBool_Check(raw)) {
        return Py_IsTrue(raw);
      }

      return false;
    }

    //! Get value as string from given variable
    std::string GetString(const std::string_view &variable) {
      if (!processed_) return "";

      if (PyObject *raw = PyDict_GetItemString(dict_, variable.data());
          raw && PyUnicode_Check(raw)) {
        return PyUnicode_AsUTF8(raw);
      }

      return "";
    }

    //! Get value as long from given variable
    uint64_t GetLong(const std::string_view &variable) {
      if (!processed_) return 0;

      if (PyObject *raw = PyDict_GetItemString(dict_, variable.data()); raw && PyLong_Check(raw)) {
        return PyLong_AsLong(raw);
      }

      return 0;
    }

   private:
    bool processed_ = false;  //!< Control flag to check if operation has been executed successfully
    PyObject *main_module_;   //!< Pointer to custom python module
    PyObject *dict_;          //!< Pointer to dictionary containing all variables from python module
  };

  /* ******************************************************************************************** */
  //! Variables

  PythonWrapper python_;  //!< Wrapper to run python code
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

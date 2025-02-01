/**
 * \file
 * \brief  Class to wrap yt-dlp funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

#include <Python.h>

#include <string>

#include "model/application_error.h"
#include "web/base/stream_fetcher.h"

namespace driver {

/**
 * @brief Class to embed python to use yt-dlp and extract streaming information from the given URL
 */
class YtDlpWrapper : public web::StreamFetcher {
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

with yt_dlp.YoutubeDL(ydl_opts) as ydl:
    info = ydl.extract_info(URL, download=False)

    parsed = json.loads(json.dumps(ydl.sanitize_info(info)))

    filtered = list(filter(lambda x: (x['resolution'] == 'audio only') and (
        x['ext'] == "m4a" or x['ext'] == "webm"), parsed["formats"]))

    filtered.sort(key=lambda x: x["quality"], reverse=True)

    title = parsed["title"]
    duration = parsed["duration"]
    streams = json.dumps(filtered))";

 public:
  /**
   * @brief Construct a new YtDlpWrapper object
   */
  YtDlpWrapper();

  /**
   * @brief Destroy the YtDlpWrapper object
   */
  virtual ~YtDlpWrapper();

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Extract streaming information from the given URL
   * @param url Endpoint address
   * @param output Song filled with streaming information from fetch operation (out)
   * @return Error code from operation
   */
  error::Code ExtractInfo(const std::string &url, model::Song &output) override;

  /* ******************************************************************************************** */
  //! Variables
 private:
  /**
   * @brief A utility struct for embedding Python interpreter in C++ applications
   * This class provides encapsulation of the Python C API for safer and more convenient
   * usage. It handles initialization, cleanup, code execution and variable retrieval.
   */
  class PythonWrapper {
   public:
    //! Initialize embedded python and create main module
    PythonWrapper() {
      Py_Initialize();
      main_module_ = PyImport_AddModule("__main__");
    }

    //! Reset module and finalize python
    ~PythonWrapper() {
      Py_DECREF(main_module_);
      Py_Finalize();
    }

    //! Execute code snippet and print any errors
    void Run(const std::string &snippet) {
      PyRun_SimpleString(snippet.c_str());
      PyErr_Print();
    }

    //! Get value as string from given variable
    std::string GetString(const std::string_view &variable) {
      PyObject *raw = PyObject_GetAttrString(main_module_, variable.data());
      return PyUnicode_AsUTF8(raw);
    }

    //! Get value as long from given variable
    uint64_t GetLong(const std::string_view &variable) {
      PyObject *raw = PyObject_GetAttrString(main_module_, variable.data());
      return PyLong_AsLong(raw);
    }

   private:
    PyObject *main_module_;
  };
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

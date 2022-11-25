
/**
 * \file
 * \brief  Class for dispatch event among blocks
 */

#ifndef INCLUDE_VIEW_BASE_EVENT_DISPATCHER_H_
#define INCLUDE_VIEW_BASE_EVENT_DISPATCHER_H_

#include <memory>

#include "model/application_error.h"
#include "view/base/block.h"
#include "view/base/custom_event.h"

namespace interface {

/**
 * @brief Interface class to dispatch events among blocks
 */
class EventDispatcher : public std::enable_shared_from_this<EventDispatcher> {
 public:
  /**
   * @brief Construct a new Event Dispatcher object
   */
  EventDispatcher() = default;

  /**
   * @brief Destroy the Event Dispatcher object
   */
  virtual ~EventDispatcher() = default;

  //! Remove these
  EventDispatcher(const EventDispatcher& other) = delete;             // copy constructor
  EventDispatcher(EventDispatcher&& other) = delete;                  // move constructor
  EventDispatcher& operator=(const EventDispatcher& other) = delete;  // copy assignment
  EventDispatcher& operator=(EventDispatcher&& other) = delete;       // move assignment

  //! Implemented by derived class
  virtual void SendEvent(const CustomEvent& event) = 0;
  virtual void ProcessEvent(const CustomEvent& event) = 0;
  virtual void SetApplicationError(error::Code id) = 0;
  virtual int CalculateNumberBars() = 0; // TODO: remove from here
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_EVENT_DISPATCHER_H_

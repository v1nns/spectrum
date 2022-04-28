/**
 * \file
 * \brief  Interface class to listen for actions from blocks
 */

#ifndef INCLUDE_VIEW_BASE_ACTION_LISTENER_H_
#define INCLUDE_VIEW_BASE_ACTION_LISTENER_H_
namespace interface {

/**
 * @brief Interface class to receive an interface action
 */
class ActionListener {
 protected:
  /**
   * @brief Construct a new ActionListener object
   */
  ActionListener() = default;

 public:
  /**
   * @brief Destroy the ActionListener object
   */
  virtual ~ActionListener() = default;

  //! Implemented by derived class
  virtual void UserSelectedFile() = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_ACTION_LISTENER_H_
#include "model/question_data.h"

#include <iomanip>

namespace model {

//! QuestionData pretty print
std::ostream& operator<<(std::ostream& out, const QuestionData& q) {
  using std::quoted;
  out << "{question: " << quoted(q.question)
      << ", cb_yes:" << quoted(q.cb_yes ? "not empty" : "empty")
      << ", cb_no:" << quoted(q.cb_no ? "not empty" : "empty") << "}";

  return out;
}

/* ********************************************************************************************** */

bool operator==(const QuestionData& lhs, const QuestionData& rhs) {
  // Do not want to check memory's address from callbacks, right?
  return lhs.question == rhs.question;
}

/* ********************************************************************************************** */

bool operator!=(const QuestionData& lhs, const QuestionData& rhs) { return !(lhs == rhs); }

}  // namespace model

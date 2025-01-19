#include "model/question_data.h"

#include <iomanip>

namespace model {

//! QuestionData pretty print
std::ostream& operator<<(std::ostream& out, const QuestionData& q) {
  out << "{question: " << std::quoted(q.question)
      << ", cb_yes:" << (q.cb_yes ? std::quoted("not empty") : std::quoted("empty"))
      << ", cb_no:" << (q.cb_no ? std::quoted("not empty") : std::quoted("empty")) << "}";

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

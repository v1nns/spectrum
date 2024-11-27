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

bool QuestionData::operator==(const QuestionData& other) const {
  // Do not want to check memory's address from callbacks, right?
  return question == other.question;
}

bool QuestionData::operator!=(const QuestionData& other) const { return !operator==(other); }

}  // namespace model

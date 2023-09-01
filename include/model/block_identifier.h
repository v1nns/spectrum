/**
 * \file
 * \brief Structure for block identification
 */

#ifndef INCLUDE_MODEL_BLOCK_IDENTIFIER_H_
#define INCLUDE_MODEL_BLOCK_IDENTIFIER_H_

#include <iostream>

namespace model {

/**
 * @brief Contains an unique ID for each existing UI block
 */
enum class BlockIdentifier {
  ListDirectory = 201,
  FileInfo = 202,
  MainTab = 203,
  MediaPlayer = 204,
  None = 205,
};

//! BlockIdentifier pretty print
std::ostream& operator<<(std::ostream& out, const BlockIdentifier& i);

}  // namespace model

#endif  // INCLUDE_MODEL_BLOCK_IDENTIFIER_H_

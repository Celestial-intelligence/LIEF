/* Copyright 2017 R. Thomas
 * Copyright 2017 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdexcept>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <functional>
#include <iterator>

#include "easylogging++.h"

#include "LIEF/visitors/Hash.hpp"

#include "LIEF/ELF/EnumToString.hpp"

#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Section.hpp"

namespace LIEF {
namespace ELF {

Section::~Section(void) = default;


Section::Section(const Elf64_Shdr* header) :
  LIEF::Section{},
  name_idx_{header->sh_name},
  type_{static_cast<SECTION_TYPES>(header->sh_type)},
  flags_{header->sh_flags},
  original_size_{header->sh_size},
  link_{header->sh_link},
  info_{header->sh_info},
  address_align_{header->sh_addralign},
  entry_size_{header->sh_entsize},
  segments_{},
  datahandler_{nullptr},
  content_c_{}
{
  this->virtual_address_ = header->sh_addr;
  this->offset_          = header->sh_offset;
  this->size_            = header->sh_size;
}

Section::Section(const Elf32_Shdr* header) :
  LIEF::Section{},
  name_idx_{header->sh_name},
  type_{static_cast<SECTION_TYPES>(header->sh_type)},
  flags_{header->sh_flags},
  original_size_{header->sh_size},
  link_{header->sh_link},
  info_{header->sh_info},
  address_align_{header->sh_addralign},
  entry_size_{header->sh_entsize},
  segments_{},
  datahandler_{nullptr},
  content_c_{}
{
  this->virtual_address_ = header->sh_addr;
  this->offset_          = header->sh_offset;
  this->size_            = header->sh_size;
}

Section::Section(void) :
  LIEF::Section{},
  name_idx_{0},
  type_{SECTION_TYPES::SHT_PROGBITS},
  flags_{0},
  original_size_{0},
  link_{0},
  info_{0},
  address_align_{16},
  entry_size_{0},
  segments_{},
  datahandler_{nullptr},
  content_c_{}
{
  this->virtual_address_ = 0;
  this->offset_          = 0;
  this->size_            = 0;
}


Section::Section(uint8_t *data, ELF_CLASS type) :
  LIEF::Section{}
{
  if (type == ELF_CLASS::ELFCLASS32) {
    *this = {reinterpret_cast<Elf32_Shdr*>(data)};
  } else if (type == ELF_CLASS::ELFCLASS64) {
    *this = {reinterpret_cast<Elf64_Shdr*>(data)};
  }
}

Section& Section::operator=(Section other) {
  this->swap(other);
  return *this;
}

Section::Section(const Section& other) :
  LIEF::Section{other},
  name_idx_{other.name_idx_},
  type_{other.type_},
  flags_{other.flags_},
  original_size_{other.original_size_},
  link_{other.link_},
  info_{other.info_},
  address_align_{other.address_align_},
  entry_size_{other.entry_size_},
  segments_{},
  datahandler_{nullptr},
  content_c_{other.content()}
{
}

void Section::swap(Section& other) {

  std::swap(this->name_,            other.name_);
  std::swap(this->virtual_address_, other.virtual_address_);
  std::swap(this->name_idx_,        other.name_idx_);
  std::swap(this->offset_,          other.offset_);
  std::swap(this->size_,            other.size_);

  std::swap(this->name_idx_,       other.name_idx_);
  std::swap(this->type_,           other.type_);
  std::swap(this->flags_,          other.flags_);
  std::swap(this->original_size_,  other.original_size_);
  std::swap(this->link_,           other.link_);
  std::swap(this->info_,           other.info_);
  std::swap(this->address_align_,  other.address_align_);
  std::swap(this->entry_size_,     other.entry_size_);
  std::swap(this->segments_,       other.segments_);
  std::swap(this->datahandler_,    other.datahandler_);
  std::swap(this->content_c_,      other.content_c_);
}

uint32_t Section::name_idx(void) const {
  return this->name_idx_;
}

SECTION_TYPES Section::type(void) const {
  return this->type_;
}

uint64_t Section::flags(void) const {
  return this->flags_;
}

bool Section::has(SECTION_FLAGS flag) const {
  return (this->flags() & static_cast<uint64_t>(flag)) != 0;
}


bool Section::has(const Segment& segment) const {

  auto&& it_segment = std::find_if(
      std::begin(this->segments_),
      std::end(this->segments_),
      [&segment] (Segment* s) {
        return *s == segment;
      });
  return it_segment != std::end(this->segments_);
}

uint64_t Section::file_offset(void) const {
  return this->offset();
}

uint64_t Section::original_size(void) const {
  return this->original_size_;
}

uint64_t Section::information(void) const {
  return this->info_;
}

uint64_t Section::entry_size(void) const {
  return this->entry_size_;
}

uint64_t Section::alignment(void) const {
  return this->address_align_;
}

std::vector<uint8_t> Section::content(void) const {
  if (this->size() == 0 or this->type() == SECTION_TYPES::SHT_NOBITS) {
    VLOG(VDEBUG) << "Section '" << this->name() << "' is empty";
    return {};
  }

  if (this->datahandler_ == nullptr) {
    VLOG(VDEBUG) << "Content from cache";
    return this->content_c_;
  } else {
    VLOG(VDEBUG) << std::hex << "Content from Data Handler [0x" << this->offset_ << ", 0x" << this->size_ << "]";
    return this->datahandler_->content(this->offset_, this->size_, DataHandler::Node::SECTION);
  }
}

uint32_t Section::link(void) const {
  return this->link_;
}

std::set<SECTION_FLAGS> Section::flags_list(void) const {
  std::set<SECTION_FLAGS> flags;
  std::copy_if(
      std::begin(section_flags_array),
      std::end(section_flags_array),
      std::inserter(flags, std::begin(flags)),
      std::bind(static_cast<bool (Section::*)(SECTION_FLAGS) const>(&Section::has), this, std::placeholders::_1));

  return flags;
}

void Section::content(const std::vector<uint8_t>& data) {
  if (this->original_size() > 0 and data.size() > this->original_size()) {
    LOG(WARNING) << "You insert data in the section "
              << this->name() << " whose the size it bigger ("
              << std::dec << data.size() << " > "
              << this->original_size_ << "). It may lead to overaly" << std::endl;
  }

  if (this->type() == SECTION_TYPES::SHT_NOBITS) {
    LOG(WARNING) << "You insert data in section "
                 << this->name() << " which has SHT_NOBITS type !" << std::endl;
  }

  if (data.size() > 0) {
    if (this->datahandler_ == nullptr) {
      this->content_c_ = data;
    } else {
      this->datahandler_->content(this->offset(), data, DataHandler::Node::SECTION);
    }
  }

  this->size_ = data.size();
}

void Section::type(SECTION_TYPES type) {
  this->type_ = type;
}

void Section::flags(uint64_t flags) {
  this->flags_ = flags;
}

void Section::add(SECTION_FLAGS flag) {
  this->flags(this->flags() | static_cast<uint64_t>(flag));
}

void Section::remove(SECTION_FLAGS flag) {
  this->flags(this->flags() & (~ static_cast<uint64_t>(flag)));
}

void Section::clear_flags(void) {
  this->flags(0);
}

void Section::file_offset(uint64_t offset) {
  this->offset(offset);
}

void Section::link(uint32_t link) {
  this->link_ = link;
}

void Section::information(uint32_t info) {
  this->info_ = info;
}

void Section::alignment(uint64_t alignment) {
  this->address_align_ = alignment;
}

void Section::entry_size(uint64_t entry_size) {
  this->entry_size_ = entry_size;
}


it_segments Section::segments(void) {
  return it_segments{std::ref(this->segments_)};
}

it_const_segments Section::segments(void) const {
  return it_const_segments{std::cref(this->segments_)};
}

void Section::accept(Visitor& visitor) const {

  LIEF::Section::accept(visitor);

  visitor.visit(this->type());
  visitor.visit(this->flags());
  visitor.visit(this->link());
  visitor.visit(this->information());
  visitor.visit(this->alignment());
  visitor.visit(this->content());
}


Section& Section::operator+=(SECTION_FLAGS c) {
  this->add(c);
  return *this;
}

Section& Section::operator-=(SECTION_FLAGS c) {
  this->remove(c);
  return *this;
}


bool Section::operator==(const Section& rhs) const {
  size_t hash_lhs = Hash::hash(*this);
  size_t hash_rhs = Hash::hash(rhs);
  return hash_lhs == hash_rhs;
}

bool Section::operator!=(const Section& rhs) const {
  return not (*this == rhs);
}


std::ostream& operator<<(std::ostream& os, const Section& section)
{
  const auto& flags = section.flags_list();
  std::string flags_str = std::accumulate(
     std::begin(flags),
     std::end(flags), std::string{},
     [] (const std::string& a, SECTION_FLAGS b) {
         return a.empty() ? to_string(b) : a + " " + to_string(b);
     });

  it_const_segments segments = section.segments();
  std::string segments_str = std::accumulate(
     std::begin(segments),
     std::end(segments), std::string{},
     [] (const std::string& a, const Segment& segment) {
         return a.empty() ? to_string(segment.type()) : a + " " + to_string(segment.type());
     });

  os << std::hex;
  os << std::left
     << std::setw(20) << section.name()
     << std::setw(15) << to_string(section.type())
     << std::setw(10) << section.virtual_address()
     << std::setw(10) << section.size()
     << std::setw(10) << section.file_offset()
     << std::setw(10) << section.entropy()
     << std::setw(30) << flags_str
     << std::setw(15) << segments_str;

  return os;
}
}
}

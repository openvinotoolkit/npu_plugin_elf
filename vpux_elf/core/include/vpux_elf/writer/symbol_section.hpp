//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/writer/symbol.hpp>
#include <vpux_elf/writer/string_section.hpp>

namespace elf {

class Writer;

namespace writer {

class SymbolSection final : public Section {
public:
    Symbol* addSymbolEntry(const std::string& name = {});
    const std::vector<std::unique_ptr<Symbol>>& getSymbols() const;

private:
    SymbolSection(const std::string& name, StringSection* namesSection);

    void finalize() override;

private:
    StringSection* m_namesSection = nullptr;
    std::vector<std::unique_ptr<Symbol>> m_symbols;

    friend Writer;
};

} // namespace writer
} // namespace elf

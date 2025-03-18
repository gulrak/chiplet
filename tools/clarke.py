#!/usr/bin/env python3
"""
Saturn Instruction Set Parser

This script parses the HP Saturn CPU instruction set definition file,
extracting operand tables and instruction definitions.
"""
import datetime
import re
import json
import string
from typing import Dict, List, Any
import argparse
from pprint import pprint


def extract_non_hex_parts(s):
    """
    Extract sequences of non-hex characters from a string.
    Only identical consecutive characters are kept together.

    Args:
        s (str): Input string containing hex and non-hex characters

    Returns:
        list: List of non-hex character sequences with identical characters grouped
    """
    result = []
    i = 0
    while i < len(s):
        char = s[i]
        if not (char.isdigit() or char.lower() in 'abcdef'):
            current_char = char
            current_sequence = char
            j = i + 1
            while j < len(s) and s[j] == current_char:
                current_sequence += s[j]
                j += 1
            result.append(current_sequence)
            i = j
        else:
            i += 1
    return result

def validate_patterns(non_hex_parts, pattern_string):
    """
    Verify that each element in non_hex_parts appears in pattern_string in the correct order,
    and each is preceded by a '<' character.

    Args:
        non_hex_parts (list): List of non-hex character sequences
        pattern_string (str): String to check for patterns

    Returns:
        bool: True if all patterns are found in the correct order, False otherwise
    """
    if not non_hex_parts:
        return True
    search_pos = 0
    for part in non_hex_parts:
        pattern_to_find = "<" + part
        found_pos = pattern_string.find(pattern_to_find, search_pos)
        if found_pos == -1:
            return False
        search_pos = found_pos + len(pattern_to_find)
    return True

def replace_patterns(non_hex_parts, pattern_string):
    """
    Replace each pattern in the form '<non_hex_part=something>' or '<non_hex_part#something>'
    with a numbered placeholder '{index}', regardless of the order in the string.

    Args:
        non_hex_parts (list): List of non-hex character sequences
        pattern_string (str): String containing patterns to replace

    Returns:
        str: Modified string with patterns replaced by numbered placeholders
    """
    if not non_hex_parts:
        return pattern_string
    result = pattern_string
    for i, part in enumerate(non_hex_parts):
        escaped_part = re.escape(part)
        pattern_regex = f"<{escaped_part}[=#][^>]*>"
        result = re.sub(pattern_regex, f"{{{i}}}", result)
    return result

def parse_instruction_variables(variables):
    """
    Parse a list of instruction variables into a dictionary.

    Args:
        variables (list): List of variable strings like 't=field', 'u=reg+12', 'yy#pcofs(3)', 'x=const(4)'

    Returns:
        dict: Dictionary mapping variable names to [operand/function name, offset/parameter]
    """
    result = {}

    for var in variables:
        if '#' in var:
            var_name, function_part = var.split('#', 1)
            if '(' in function_part and ')' in function_part:
                function_name = function_part.split('(', 1)[0]
                param_str = function_part.split('(', 1)[1].rstrip(')')
                try:
                    param = int(param_str)
                except ValueError:
                    param = 0
            else:
                function_name = function_part
                param = 0
            result[var_name] = [function_name, param]
        elif '=' in var:
            var_name, operand_part = var.split('=', 1)
            if '+' in operand_part:
                operand_name, offset_str = operand_part.split('+', 1)
                try:
                    offset = int(offset_str)
                except ValueError:
                    offset = 0
            else:
                if '(' in operand_part and ')' in operand_part:
                    operand_name = operand_part.split('(', 1)[0]
                    param_str = operand_part.split('(', 1)[1].rstrip(')')
                    try:
                        offset = int(param_str)
                    except ValueError:
                        offset = 0
                else:
                    operand_name = operand_part
                    offset = 0
            result[var_name] = [operand_name, offset]
    return result

def make_valid_identifier(s):
    """
    Convert a string to a valid Python identifier by:
    1. Replacing non-alphanumeric characters with underscores
    2. Removing consecutive underscores
    3. Removing leading and trailing underscores

    Args:
        s (str): Input string

    Returns:
        str: Valid identifier
    """
    if not s:
        return "empty"  # Return a default for empty strings
    result = ""
    for char in s:
        if char.isalnum():
            result += char
        else:
            result += "_"
    while "__" in result:
        result = result.replace("__", "_")
    result = result.strip("_")
    if not result:
        return "empty"
    return result

class SaturnParser:
    def __init__(self, filename: str):
        self.filename = filename
        self.operand_tables: Dict[str, Dict[str, int]] = {}
        self.instructions: List[Dict[str, Any]] = []
        self.alias_instructions: List[Dict[str, str]] = []
        self.decode_tree: Dict[str, Any] = {}
        self.decode_tree_array: List[Dict[str, Any]] = []
        self.decode_queue: List[Dict[str, Any]] = []
        self.next_table_index: int = 0
        self.word_table_size: int = 0

    def parse(self) -> None:
        try:
            with open(self.filename, 'r') as f:
                content = f.read()
        except FileNotFoundError:
            print(f"Error: File {self.filename} not found.")
            return
        except Exception as e:
            print(f"Error reading file: {e}")
            return

        current_section = None
        current_operand_table = {}

        lines = content.split('\n')
        for line_num, line in enumerate(lines, 1):
            if line.strip().startswith(';') or not line.strip():
                continue

            if line.strip().startswith('operand '):
                if current_operand_table and current_section:
                    self.operand_tables[current_section] = current_operand_table
                    current_operand_table = {}
                current_section = line.strip().split(' ', 1)[1]
                continue

            if line.strip() == 'endoperand':
                if current_operand_table and current_section:
                    self.operand_tables[current_section] = current_operand_table
                    current_operand_table = {}
                current_section = None
                continue

            if line.strip() == 'instructions':
                current_section = 'instructions'
                continue

            if current_section and current_section != 'instructions':
                parts = line.strip().split()
                if len(parts) >= 2:
                    operand_name = parts[0]
                    operand_value = None

                    try:
                        if len(parts[1]) > 1 and parts[1].lower().startswith('0x'):
                            operand_value = int(parts[1], 16)
                        elif parts[1].lower() in 'abcdef':
                            operand_value = int(parts[1], 16)
                        else:
                            operand_value = int(parts[1])
                    except ValueError:
                        print(f"Warning: Invalid operand value at line {line_num}: {line}")
                        continue

                    current_operand_table[operand_name] = operand_value
                continue

            if current_section == 'instructions':
                is_rev = False
                if line.endswith('; alias'):
                    is_rev = True
                line = line.split(';', 1)[0]
                parts = line.strip().split(None, 1)
                if len(parts) >= 2:
                    opcode_pattern = parts[0]
                    instruction_pattern = parts[1]
                    instr_parts = {'opcode': opcode_pattern}
                    mnemonic_parts = instruction_pattern.split(None, 1)
                    if len(mnemonic_parts) > 0:
                        instr_parts['mnemonic'] = mnemonic_parts[0]

                        if len(mnemonic_parts) > 1:
                            instr_parts['operands'] = mnemonic_parts[1]

                    variables = []
                    var_pattern = r'<([^>]+)>'
                    for match in re.finditer(var_pattern, instruction_pattern):
                        var_def = match.group(1)
                        variables.append(var_def)

                    if variables:
                        instr_parts['variables'] = variables

                    func = instr_parts['opcode'] + '_' + instr_parts['mnemonic']
                    if 'operands' in instr_parts:
                        func += '_' + instr_parts['operands']
                    instr_parts['identifier'] = make_valid_identifier(func)

                    if is_rev:
                        self.alias_instructions.append(instr_parts)
                    else:
                        instr_parts['index'] = len(self.instructions)
                        self.instructions.append(instr_parts)

    def get_operand_tables(self) -> Dict[str, Dict[str, int]]:
        return self.operand_tables

    def get_instructions(self) -> List[Dict[str, Any]]:
        return self.instructions

    def print_summary(self) -> None:
        print(f"Parsed {len(self.operand_tables)} operand tables:")
        for table_name, table in self.operand_tables.items():
            print(f"  - {table_name}: {len(table)} entries")

        print(f"\nParsed {len(self.instructions)} instructions")

        mnemonic_families = {}
        for instr in self.instructions:
            if 'mnemonic' in instr:
                family = instr['mnemonic'].split('.')[0] if '.' in instr['mnemonic'] else instr['mnemonic']
                mnemonic_families[family] = mnemonic_families.get(family, 0) + 1

        print("\nInstruction families:")
        for family, count in sorted(mnemonic_families.items(), key=lambda x: x[1], reverse=True):
            print(f"  - {family}: {count}")

    def analyze_opcodes(self) -> None:
        print("\nOpcode Pattern Analysis:")
        fixed_length = 0
        variable_length = 0
        opcode_lengths = {}

        for instr in self.instructions:
            opcode = instr.get('opcode', '')

            hex_count = sum(1 for c in opcode if c in '0123456789abcdef')
            var_count = len(opcode) - hex_count

            if var_count == 0:
                fixed_length += 1
            else:
                variable_length += 1

            length = len(opcode)
            opcode_lengths[length] = opcode_lengths.get(length, 0) + 1

        print(f"  - Fixed-length opcodes: {fixed_length}")
        print(f"  - Variable-length opcodes: {variable_length}")
        print("\nOpcode length distribution:")
        for length, count in sorted(opcode_lengths.items()):
            print(f"  - {length} nibbles: {count} instructions")

    def analyze_operand_usage(self) -> None:
        print("\nOperand Table Usage:")

        table_usage = {table: 0 for table in self.operand_tables}

        for instr in self.instructions:
            if 'variables' in instr:
                for var_def in instr['variables']:
                    if '=' in var_def:
                        _, operand_type = var_def.split('=', 1)

                        if '(' in operand_type:
                            operand_type = operand_type.split('(', 1)[0]

                        if '+' in operand_type:
                            operand_type = operand_type.split('+', 1)[0]

                        if operand_type in table_usage:
                            table_usage[operand_type] += 1

        for table, count in sorted(table_usage.items(), key=lambda x: x[1], reverse=True):
            if count > 0:
                print(f"  - {table}: used in {count} instructions")

    def save_to_json(self, filename: str) -> None:
        data = {
            'operand_tables': self.operand_tables,
            'instructions': self.instructions
        }

        try:
            with open(filename, 'w') as f:
                json.dump(data, f, indent=2)
            print(f"\nData saved to {filename}")
        except Exception as e:
            print(f"Error saving data: {e}")

    def insert_instruction_into_tree(self, path:str, tree:Dict[str, Any], instr:Dict[str, Any], nibble:str, nibble_index: int) -> None:
        if nibble not in string.hexdigits:
            for var in instr['variables']:
                if var.startswith(nibble):
                    var_name, operand_type = re.split('[=#]', var)
                    offset = 0
                    if '+' in operand_type:
                        operand_type, offset = operand_type.split('+', 1)
                        offset = int(offset)
                    if operand_type in self.operand_tables:
                        for key, value in self.operand_tables[operand_type].items():
                            self.insert_instruction_into_tree(path, tree, instr, f'{value+offset:x}', nibble_index)
                        return
                    elif operand_type == 'const(4)' or operand_type == 'hwflags()':
                        print(f"expanding rule...")
                        for value in range(16):
                            self.insert_instruction_into_tree(path, tree, instr, f'{value:x}', nibble_index)
                        return
                    elif operand_type.startswith('pcofs('):
                        print(f"expanding rule...")
                        for value in range(16):
                            if value != 0 and instr['opcode'][:len(var_name)] != var_name:
                                self.insert_instruction_into_tree(path, tree, instr, f'{value:x}', nibble_index)
                        return
                    else:
                        raise ValueError(f"Invalid operand type: {operand_type}")
            raise ValueError(f"Invalid nibble: {nibble}")

        current_path = path + nibble
        if nibble not in tree:
            print(f"Adding instruction at {current_path}: {instr['opcode']}...")
            tree[nibble] = instr['index']
        else:
            if isinstance(tree[nibble], int):
                temp = tree[nibble]
                temp_instr = self.instructions[temp]
                tree[nibble] = {}
                print(f"Relocating instruction at {current_path}: {temp_instr} for {instr['opcode']}...")
                self.insert_instruction_into_tree(current_path, tree[nibble], temp_instr, temp_instr['opcode'][nibble_index+1], nibble_index+1)
            self.insert_instruction_into_tree(current_path, tree[nibble], instr, instr['opcode'][nibble_index+1], nibble_index+1)

    def generate_decode_tree(self):
        for instr in self.instructions:
            opcode = instr['opcode']
            if opcode[0] in string.hexdigits:
                self.insert_instruction_into_tree("", self.decode_tree, instr, instr['opcode'][0], 0)
        print(json.dumps(self.decode_tree, indent=2))

    def dump_decode_tree_as_array(self, file, path:str, tree:Dict[str, Any], compact:bool) -> None:
        if compact:
            file.write("   ")
        else:
            file.write(f"    // {self.word_table_size/2}: {path}\n")
        for nibble in '0123456789abcdef':
            self.word_table_size += 2
            if compact:
                if nibble in tree:
                    if isinstance(tree[nibble], int):
                        instr = self.instructions[tree[nibble]]
                        file.write(f" {tree[nibble]},")
                    else:
                        self.next_table_index += 16
                        self.decode_queue.append({'path':path + nibble, 'index': self.next_table_index, 'tree': tree[nibble]})
                        file.write(f" {-self.next_table_index},")
                else:
                    file.write(" 0,")
            else:
                if nibble in tree:
                    if isinstance(tree[nibble], int):
                        instr = self.instructions[tree[nibble]]
                        file.write(f"    {tree[nibble]}, // {path + nibble} -> {instr['opcode']} ({instr['mnemonic']})\n")
                    else:
                        self.next_table_index += 16
                        self.decode_queue.append({'path':path + nibble, 'index': self.next_table_index, 'tree': tree[nibble]})
                        file.write(f"    {-self.next_table_index}, // {path + nibble}\n")
                else:
                    file.write(f"    0, // {path + nibble}\n")
        if compact:
            file.write("\n")

    def generate_source(self, compact:bool) -> None:
        now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
        used_opcodes = {1, 2, 3, 5, 6, 7, 8, 9, 10, 13, 20, 23, 24, 26, 31, 32, 33, 34, 35, 36, 38, 39, 42, 43, 50, 51,
                        54, 55, 57, 71, 72, 73, 74, 75, 76, 78, 82, 83, 87, 88, 89, 92, 94, 95, 96, 97, 100, 104, 105,
                        106, 107, 108, 109, 110, 111, 112, 114, 118, 121, 122, 124, 125, 126, 127, 134, 135, 136, 138,
                        141, 150, 151, 152, 154, 172, 173, 174, 183, 184, 186, 188, 189, 190, 191, 192, 193, 201, 202,
                        203, 204, 205, 208, 209, 210, 211, 214, 217, 218}
        with open(f'saturndecoder.hpp', 'w') as f:
            f.write(f"// Auto-generated {now} by {__file__}\n\n")
            f.write("""#pragma once

#include <cstdint>
#include <string>

namespace emu {

class SaturnDecoder {
public:
    struct InstructionInfo {
        const char* fmt;
        uint32_t opcode;
        int8_t opcodeSize;
        uint8_t numVars;
        uint8_t varSizes[3];
        uint8_t varOffsets[3];
        uint8_t varTypes[3];
        uint8_t varArgs[3];
    };
    
    
    enum OpcodeId
    {
""")
            for instr in self.instructions:
                if instr['opcode'][0] in string.hexdigits:
                    f.write(f"        Opc_{instr['identifier']},\n")
            f.write("""        Opc_Invalid
    };

    struct DecodeResult
    {
        OpcodeId oid;
        const InstructionInfo* info;
        uint32_t opcode;
        uint64_t varArg;
    };

    SaturnDecoder() = default;
    virtual ~SaturnDecoder() = default;
    virtual unsigned readNibble(const uint32_t address) const = 0;
    uint64_t readNibbles(int n, uint32_t& address) const
    {
        uint64_t result = 0;
        for (uint32_t i = 0; i < n; ++i) {
            result |= readNibble(address++) << (i*4);
        }
        return result;
    }
    template <int N>
    uint64_t readNibbles(uint32_t& address) const
    {
        uint64_t result = 0;
        for (uint32_t i = 0; i < N; ++i) {
            result |= readNibble(address++) << (i*4);
        }
        return result;
    }
    DecodeResult decode(uint32_t& address) const;
    static std::string varToString(const DecodeResult& decoded, uint32_t startAddress, unsigned index);
};

}
""")

        with open(f'saturndecoder.cpp', 'w') as f:
            f.write(f"// Auto-generated {now} by {__file__}\n\n#include \"saturndecoder.hpp\"\n\n")
            f.write("""
#include <cstdint>
#include <functional>
#include <string>

#include <fmt/format.h>

namespace emu {

static constexpr int16_t decodingTable[] = {
""")
            self.generate_decode_tree()
            self.decode_queue.append({'path':'', 'index': 0, 'tree': self.decode_tree})
            while len(self.decode_queue) > 0:
                item = self.decode_queue.pop(0)
                self.dump_decode_tree_as_array(f, item['path'], item['tree'], compact)

            f.write("};\n\n")

            print(f"Word table size: {self.word_table_size}")

            for var_name, table in self.operand_tables.items():
                f.write(f"static constexpr std::string_view {var_name}[] = {{")
                for index in range(max(table.values()) + 1):
                    matching_keys = [k for k, v in table.items() if v == index]
                    if matching_keys:
                        f.write(f"\"{matching_keys[0]}\",")
                    else:
                        f.write(f"\"???\",")
                f.write("};\n")

            f.write("static constexpr const std::string_view* varTables[] = {\n")
            for var_name, table in self.operand_tables.items():
                f.write(f"    {var_name},\n")
            f.write("};\n")


            f.write("\nenum VariableType {\n")
            for var_name, table in self.operand_tables.items():
                f.write(f"    vt_{var_name},\n")
            f.write(f"    vt_maxtables,\n")
            f.write("    vt_const = vt_maxtables,\n    vt_nzconst,\n    vt_varconst,\n    vt_pcofs,\n    vt_hwflags,\n    vt_none\n")
            f.write("};\n\n")

            f.write(f"\nstatic constexpr SaturnDecoder::InstructionInfo instructions[] = {{\n")
            for instr in self.instructions:
                if instr['opcode'][0] in string.hexdigits:
                    fmt = instr['mnemonic'] if not 'operands' in instr else instr['mnemonic'] + ' ' + instr['operands']
                    opcode_vars = extract_non_hex_parts(instr['opcode'])
                    if opcode_vars:
                        fmt = replace_patterns(opcode_vars, fmt)
                    opcode_size = len(instr['opcode'])
                    opcode = int(re.sub(r'[^0-9A-Fa-f]', '0', instr['opcode']), 16)
                    if instr['opcode'] in ['3ix', '8082ix']:
                        if instr['opcode'] == '3ix':
                            opcode_size = -2
                            opcode = 0x30
                        else:
                            opcode_size = -5
                            opcode = 0x80820
                    f.write(f"    {{ \"{fmt}\", 0x{opcode:x}, {opcode_size}, {len(opcode_vars)}, ")
                    f.write(f"{{{','.join(str(len(opcode_vars[i])) if i < len(opcode_vars) else '0' for i in range(3))}}},")
                    f.write(f"{{{','.join(str(instr['opcode'].find(opcode_vars[i])) if i < len(opcode_vars) else '0' for i in range(3))}}}, ")
                    f.write("{")
                    parsed_vars = {}
                    if 'variables' in instr:
                        parsed_vars = parse_instruction_variables(instr['variables'])
                        first = True
                        for pattern in opcode_vars:
                            if first:
                                first = False
                            else:
                                f.write(", ")
                            if pattern in parsed_vars:
                                f.write(f"vt_{parsed_vars[pattern][0]}")
                            else:
                                f.write("vt_none")
                        for i in range(3-len(opcode_vars)):
                            if first:
                                first = False
                            else:
                                f.write(", ")
                            f.write("vt_none")
                    else:
                        f.write("vt_none, vt_none, vt_none")
                    f.write("}, {")
                    if parsed_vars:
                        first = True
                        for pattern in opcode_vars:
                            if first:
                                first = False
                            else:
                                f.write(", ")
                            if pattern in parsed_vars:
                                f.write(f"{parsed_vars[pattern][1]}")
                            else:
                                f.write("0")
                        for i in range(3-len(opcode_vars)):
                            if first:
                                first = False
                            else:
                                f.write(", ")
                            f.write("0")
                    else:
                        f.write("0, 0, 0")

                    f.write("} },\n")
            f.write("\n};\n\n")

            f.write("""
SaturnDecoder::DecodeResult SaturnDecoder::decode(uint32_t& address) const
{
    uint32_t startAddress = address;
    unsigned nibble = readNibbles<1>(address);
    uint32_t opcode = nibble;
    uint64_t varArg = 0;
    size_t index = 0;
    while (decodingTable[index + nibble] < 0) {
        index = -decodingTable[index + nibble];
        nibble = readNibble(address++);
        opcode = (opcode << 4) | nibble;
    }
    if (decodingTable[index + nibble] == 0) {
        address = startAddress;
        return {Opc_Invalid, nullptr, 0, 0};
    }
    auto instructionIndex = decodingTable[index + nibble];
    auto& info = instructions[instructionIndex];
    if (info.opcodeSize < 0) {
        nibble = readNibble(address++);
        opcode = (opcode << 4) | nibble;
        for (unsigned i = 0; i <= nibble; i++) {
            varArg = (varArg << 4) | readNibble(address++);
        }        
    }
    else {
        if (address - startAddress < info.opcodeSize) {
            auto missing = info.opcodeSize - (address - startAddress);
            for (unsigned i = 0; i < missing; i++) {
                opcode = (opcode << 4) | readNibble(address++);
            }
        }
    }
    return {static_cast<OpcodeId>(instructionIndex), &info, opcode, varArg};
}

static int64_t twosComplement(const uint64_t value, const unsigned bitSize)
{
    if (value & (1ULL << (bitSize - 1))) {
        return static_cast<int64_t>(value) - (1LL << bitSize);
    }
    return static_cast<int64_t>(value);
}

static uint64_t reverseNibbles(uint64_t value, int n) {
    uint64_t result = 0;
    for (int i = 0; i < n; i++) {
        uint8_t nibble = (value >> (i * 4)) & 0xF;
        result |= (uint64_t)nibble << ((n - 1 - i) * 4);
    }
    return result;
}

std::string SaturnDecoder::varToString(const DecodeResult& decoded, uint32_t startAddress, unsigned index)
{
    const auto& info = *decoded.info;
    if (info.varTypes[index] == vt_none) {
        return {};
    }
    if (info.opcodeSize < 0) {
        auto varSize = (decoded.opcode & 0xF) + 1;
        return index ? fmt::format("{}", varSize) : fmt::format("#{:0{}x}", decoded.varArg, varSize);
    }
    auto varSize = info.varSizes[index]*4;
    auto val = varSize ? (decoded.opcode >> (info.opcodeSize*4 - (info.varOffsets[index]*4 + varSize))) & ((1ULL << varSize) - 1) : 0;
    val = reverseNibbles(val, info.varSizes[index]);
    if (info.varTypes[index] < vt_maxtables) {
        return std::string(varTables[info.varTypes[index]][val - info.varArgs[index]]);
    }
    switch (info.varTypes[index]) {
        case vt_const:
            return val < 10 ? fmt::format("{}", val) : fmt::format("#{:0{}x}", val, varSize>>2);
        case vt_nzconst:
            return fmt::format("{}", val+1);
        case vt_pcofs: {
            auto distance = twosComplement(val, varSize);
            return fmt::format("#{:05x}", startAddress + distance + info.varArgs[index]);
        }
        case vt_hwflags:
            return "hwflags???";
        default:
            return "???";
    }
}

""")
            f.write("\n}\n")

        with open(f'saturnexecutor.hpp', 'w') as f:
            f.write(f"// Auto-generated {now} by {__file__}\n\n")
            f.write("""#pragma once

#include "saturndecoder.hpp"

#include <array>
#include <cstdint>

namespace emu {

class SaturnExecutor : public SaturnDecoder {
public:
    SaturnExecutor() = default;
    ~SaturnExecutor() override = default;

""")
            sorted_list = sorted(self.instructions, key=lambda d: d['mnemonic'])
            for instr in sorted_list:
                if instr['opcode'][0] not in string.hexdigits:
                    continue
                #f.write(f"    void op_{'_'.join([instr['opcode']] + [m.group(0) for m in [re.match(r'[0-9A-Za-z]+', instr['mnemonic'])] if m])}();\n")
                comment = ""
                if instr['index'] in used_opcodes:
                    comment = " // used"
                f.write(f"    void op_{instr['identifier']}(const DecodeResult& decoded);{comment}\n")

            f.write("""    void execute(const DecodeResult& decoded)
    {
        switch(decoded.oid) {
""")

            for instr in self.instructions:
                if instr['opcode'][0] in string.hexdigits:
                    f.write(f"        case Opc_{instr['identifier']}: op_{instr['identifier']}(decoded); break;\n")
            f.write("""        default: break;
        }
    }
private:
    uint64_t _rA{};
    uint64_t _rB{};
    uint64_t _rC{};
    uint64_t _rD{};
    std::array<uint64_t,5> _rR{};
    std::array<uint32_t,8> _rRSTK{};
    uint16_t _rIN{};    // 10 bit
    uint16_t _rOUT{};   // 10 bit
    uint32_t _rPC{};
    uint32_t _rD0{};
    uint32_t _rD1{};
    uint16_t _rST{};
    uint8_t _rP{};
    uint8_t _rHS{};
    bool _rCarry{false};
""")

            f.write("};\n\n}\n")


def main():
    parser = argparse.ArgumentParser(description='Parse Saturn instruction set definition file')
    parser.add_argument('filename', help='Path to the instruction set definition file')
    parser.add_argument('-o', '--output', help='Output JSON file', default='saturn_instr.json')
    parser.add_argument('-d', '--dump', action='store_true', help='Dump full parsed data')
    parser.add_argument('-c', '--code', action='store_true', help='Generate C++ source')
    parser.add_argument('--compact', action='store_true', help='Generate compact decoding table')
    args = parser.parse_args()

    saturn_parser = SaturnParser(args.filename)
    saturn_parser.parse()

    if args.dump:
        print("\nOperand Tables:")
        pprint(saturn_parser.get_operand_tables())
        print("\nInstructions:")
        pprint(saturn_parser.get_instructions())
    else:
        saturn_parser.print_summary()
        saturn_parser.analyze_opcodes()
        saturn_parser.analyze_operand_usage()

    if args.code:
        saturn_parser.generate_source(args.compact)

    saturn_parser.save_to_json(args.output)


if __name__ == "__main__":
    main()
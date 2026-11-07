#include "emu/nes.hpp"
#include <sstream>
#include <stdexcept>
namespace tzj::emu {
namespace {
constexpr Byte C = 1, Z = 2, I = 4, D = 8, B = 16, U = 32, V = 64, N = 128;
}
Byte Nes::fetch() {
    return read(cpu_.pc++);
}
Word Nes::fetch_word() {
    Byte low = fetch();
    return Word(low | (fetch() << 8));
}
void Nes::push(Byte value) {
    write(Word(0x100 | cpu_.sp--), value);
}
Byte Nes::pop() {
    return read(Word(0x100 | ++cpu_.sp));
}
void Nes::flag(Byte bit, bool value) {
    cpu_.status = Byte(value ? cpu_.status | bit : cpu_.status & ~bit);
}
void Nes::flags(Byte value) {
    flag(Z, value == 0);
    flag(N, (value & 0x80) != 0);
}
void Nes::compare(Byte lhs, Byte rhs) {
    flag(C, lhs >= rhs);
    flags(Byte(lhs - rhs));
}
void Nes::add(Byte value) {
    int sum = cpu_.a + value + (cpu_.status & C);
    flag(V, ((~(cpu_.a ^ value) & (cpu_.a ^ sum)) & 0x80) != 0);
    flag(C, sum > 255);
    cpu_.a = Byte(sum);
    flags(cpu_.a);
}
void Nes::interrupt(Word vector, bool software) {
    push(Byte(cpu_.pc >> 8));
    push(Byte(cpu_.pc));
    push(Byte((cpu_.status | U | (software ? B : 0)) & Byte(software ? 255 : ~B)));
    flag(I, true);
    cpu_.pc = read_word(vector);
}
int Nes::cpu_step() {
    if (nmi_) {
        nmi_ = false;
        interrupt(0xfffa, false);
        return 7;
    }
    const Byte opcode = fetch();
    auto zero_indexed = [&](Byte index) { return Word(Byte(fetch() + index)); };
    auto indexed = [&](Byte index, bool &cross) {
        Word base = fetch_word(), result = Word(base + index);
        cross = (base & 0xff00) != (result & 0xff00);
        return result;
    };
    auto branch = [&]() {
        constexpr Byte bits[]{N, N, V, V, C, C, Z, Z};
        unsigned kind = opcode >> 5;
        auto displacement = static_cast<std::int8_t>(fetch());
        if (bool(cpu_.status & bits[kind]) != bool(kind & 1))
            return 2;
        Word old = cpu_.pc;
        cpu_.pc = Word(cpu_.pc + displacement);
        return 3 + int((old & 0xff00) != (cpu_.pc & 0xff00));
    };
    if ((opcode & 0x1f) == 0x10)
        return branch();
    // Control flow, stack, transfers and the non-uniform instruction group.
    switch (opcode) {
    case 0x00:
        ++cpu_.pc;
        interrupt(0xfffe, true);
        return 7;
    case 0x08:
        push(Byte(cpu_.status | B | U));
        return 3;
    case 0x18:
        flag(C, false);
        return 2;
    case 0x20: {
        auto target = fetch_word();
        Word back = Word(cpu_.pc - 1);
        push(Byte(back >> 8));
        push(Byte(back));
        cpu_.pc = target;
        return 6;
    }
    case 0x24:
    case 0x2c: {
        Byte value = read(opcode == 0x24 ? Word(fetch()) : fetch_word());
        flag(Z, (cpu_.a & value) == 0);
        flag(N, value & N);
        flag(V, value & V);
        return opcode == 0x24 ? 3 : 4;
    }
    case 0x28:
        cpu_.status = Byte((pop() & ~B) | U);
        return 4;
    case 0x38:
        flag(C, true);
        return 2;
    case 0x40: {
        cpu_.status = Byte((pop() & ~B) | U);
        Byte low = pop();
        cpu_.pc = Word(low | (pop() << 8));
        return 6;
    }
    case 0x48:
        push(cpu_.a);
        return 3;
    case 0x4c:
        cpu_.pc = fetch_word();
        return 3;
    case 0x58:
        flag(I, false);
        return 2;
    case 0x60: {
        Byte low = pop();
        cpu_.pc = Word((low | (pop() << 8)) + 1);
        return 6;
    }
    case 0x68:
        cpu_.a = pop();
        flags(cpu_.a);
        return 4;
    case 0x6c: {
        auto ptr = fetch_word();
        Byte low = read(ptr);
        cpu_.pc = Word(low | (read(Word((ptr & 0xff00) | Byte(ptr + 1))) << 8));
        return 5;
    }
    case 0x78:
        flag(I, true);
        return 2;
    case 0x84:
        write(fetch(), cpu_.y);
        return 3;
    case 0x88:
        flags(--cpu_.y);
        return 2;
    case 0x8a:
        cpu_.a = cpu_.x;
        flags(cpu_.a);
        return 2;
    case 0x8c:
        write(fetch_word(), cpu_.y);
        return 4;
    case 0x94:
        write(zero_indexed(cpu_.x), cpu_.y);
        return 4;
    case 0x98:
        cpu_.a = cpu_.y;
        flags(cpu_.a);
        return 2;
    case 0x9a:
        cpu_.sp = cpu_.x;
        return 2;
    case 0xa0:
        cpu_.y = fetch();
        flags(cpu_.y);
        return 2;
    case 0xa4:
        cpu_.y = read(fetch());
        flags(cpu_.y);
        return 3;
    case 0xa8:
        cpu_.y = cpu_.a;
        flags(cpu_.y);
        return 2;
    case 0xaa:
        cpu_.x = cpu_.a;
        flags(cpu_.x);
        return 2;
    case 0xac:
        cpu_.y = read(fetch_word());
        flags(cpu_.y);
        return 4;
    case 0xb4:
        cpu_.y = read(zero_indexed(cpu_.x));
        flags(cpu_.y);
        return 4;
    case 0xb8:
        flag(V, false);
        return 2;
    case 0xba:
        cpu_.x = cpu_.sp;
        flags(cpu_.x);
        return 2;
    case 0xbc: {
        bool cross = false;
        auto address = indexed(cpu_.x, cross);
        cpu_.y = read(address);
        flags(cpu_.y);
        return 4 + int(cross);
    }
    case 0xc0:
        compare(cpu_.y, fetch());
        return 2;
    case 0xc4:
        compare(cpu_.y, read(fetch()));
        return 3;
    case 0xc8:
        flags(++cpu_.y);
        return 2;
    case 0xca:
        flags(--cpu_.x);
        return 2;
    case 0xcc:
        compare(cpu_.y, read(fetch_word()));
        return 4;
    case 0xd8:
        flag(D, false);
        return 2;
    case 0xe0:
        compare(cpu_.x, fetch());
        return 2;
    case 0xe4:
        compare(cpu_.x, read(fetch()));
        return 3;
    case 0xe8:
        flags(++cpu_.x);
        return 2;
    case 0xea:
        return 2;
    case 0xec:
        compare(cpu_.x, read(fetch_word()));
        return 4;
    case 0xf8:
        flag(D, true);
        return 2;
    default:
        break;
    }
    if ((opcode & 3) == 1) {
        unsigned operation = opcode >> 5, mode = (opcode >> 2) & 7;
        if (!(operation == 4 && mode == 2)) {
            bool cross = false;
            Word address = 0;
            constexpr int cycles[]{6, 3, 2, 4, 5, 4, 4, 4};
            switch (mode) {
            case 0: {
                Byte ptr = Byte(fetch() + cpu_.x);
                address = Word(read(ptr) | (read(Byte(ptr + 1)) << 8));
                break;
            }
            case 1:
                address = fetch();
                break;
            case 2:
                address = cpu_.pc++;
                break;
            case 3:
                address = fetch_word();
                break;
            case 4: {
                Byte ptr = fetch();
                Word base = Word(read(ptr) | (read(Byte(ptr + 1)) << 8));
                address = Word(base + cpu_.y);
                cross = (base & 0xff00) != (address & 0xff00);
                break;
            }
            case 5:
                address = zero_indexed(cpu_.x);
                break;
            case 6:
                address = indexed(cpu_.y, cross);
                break;
            case 7:
                address = indexed(cpu_.x, cross);
                break;
            }
            if (operation == 4) {
                write(address, cpu_.a);
                return cycles[mode] + int(mode == 4 || mode == 6 || mode == 7);
            }
            Byte value = read(address);
            switch (operation) {
            case 0:
                cpu_.a |= value;
                flags(cpu_.a);
                break;
            case 1:
                cpu_.a &= value;
                flags(cpu_.a);
                break;
            case 2:
                cpu_.a ^= value;
                flags(cpu_.a);
                break;
            case 3:
                add(value);
                break;
            case 5:
                cpu_.a = value;
                flags(cpu_.a);
                break;
            case 6:
                compare(cpu_.a, value);
                break;
            case 7:
                add(Byte(~value));
                break;
            }
            return cycles[mode] + int(cross && (mode == 4 || mode == 6 || mode == 7));
        }
    }
    if ((opcode & 3) == 2) {
        unsigned operation = opcode >> 5, mode = (opcode >> 2) & 7;
        if (operation == 5 && mode == 0) {
            cpu_.x = fetch();
            flags(cpu_.x);
            return 2;
        }
        const bool accumulator = mode == 2 && operation < 4;
        if (accumulator || mode == 1 || mode == 3 || mode == 5 || (mode == 7 && operation != 4)) {
            bool cross = false;
            Word address = 0;
            int cycles = 2;
            switch (mode) {
            case 1:
                address = fetch();
                cycles = 3;
                break;
            case 3:
                address = fetch_word();
                cycles = 4;
                break;
            case 5:
                address = zero_indexed(operation == 4 || operation == 5 ? cpu_.y : cpu_.x);
                cycles = 4;
                break;
            case 7:
                address = indexed(operation == 5 ? cpu_.y : cpu_.x, cross);
                cycles = 4 + int(cross);
                break;
            default:
                break;
            }
            if (operation == 4) {
                write(address, cpu_.x);
                return cycles;
            }
            Byte value = accumulator ? cpu_.a : read(address);
            if (operation == 5) {
                cpu_.x = value;
                flags(value);
                return cycles;
            }
            const bool carry = (cpu_.status & C) != 0;
            // RMW instructions write the original byte before the modified byte.
            if (!accumulator)
                write(address, value);
            switch (operation) {
            case 0:
                flag(C, value & 0x80);
                value = Byte(value << 1);
                break;
            case 1:
                flag(C, value & 0x80);
                value = Byte((value << 1) | int(carry));
                break;
            case 2:
                flag(C, value & 1);
                value = Byte(value >> 1);
                break;
            case 3:
                flag(C, value & 1);
                value = Byte((value >> 1) | (carry ? 0x80 : 0));
                break;
            case 6:
                --value;
                break;
            case 7:
                ++value;
                break;
            }
            flags(value);
            if (accumulator)
                cpu_.a = value;
            else
                write(address, value);
            return accumulator ? 2 : mode == 7 ? 7 : cycles + 2;
        }
    }
    std::ostringstream message;
    message << "Unsupported 6502 opcode $" << std::hex << int(opcode) << " at $"
            << Word(cpu_.pc - 1);
    throw std::runtime_error(message.str());
}
} // namespace tzj::emu

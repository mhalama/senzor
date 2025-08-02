package cz.zorn.iruploader.parser

@JvmInline
value class Reg32 private constructor(val reg: Int) {
    companion object {
        fun of(value: Int): Reg32 {
            require(value in 0..32) { "Value must be between 0 and 32, was $value" }
            return Reg32(value)
        }
    }
}

@JvmInline
value class Const8 private constructor(val value: Int) {
    companion object {
        fun of(value: Int): Const8 {
            require(value in 0..7) { "Value must be between 0 and 7, was $value" }
            return Const8(value)
        }
    }
}

enum class InstructionOptions {
    NO_OPTION,
    DF_NO_EIJMP,
    DF_NO_EICALL,
    DF_NO_SPM,
    DF_NO_ESPM,
    DF_NO_BREAK,
    DF_NO_LPM,
    DF_NO_ELPM,
    DF_TINY1X
}

sealed class Instruction(
    val opcode: Int,
    options: InstructionOptions = InstructionOptions.NO_OPTION
) {
    object NOP : Instruction(opcode = 0x0000)
    object SEC : Instruction(opcode = 0x9408)
    object CLC : Instruction(opcode = 0x9488)
    object SEN : Instruction(opcode = 0x9428)
    object CLN : Instruction(opcode = 0x94a8)
    object SEZ : Instruction(opcode = 0x9418)
    object CLZ : Instruction(opcode = 0x9498)
    object SEI : Instruction(opcode = 0x9478)
    object CLI : Instruction(opcode = 0x94f8)
    object SES : Instruction(opcode = 0x9448)
    object CLS : Instruction(opcode = 0x94c8)
    object SEV : Instruction(opcode = 0x9438)
    object CLV : Instruction(opcode = 0x94b8)
    object SET : Instruction(opcode = 0x9468)
    object CLT : Instruction(opcode = 0x94e8)
    object SEH : Instruction(opcode = 0x9458)
    object CLH : Instruction(opcode = 0x94d8)
    object SLEEP : Instruction(opcode = 0x9588)
    object WDR : Instruction(opcode = 0x95a8)

    object IJMP : Instruction(opcode = 0x9409, InstructionOptions.DF_TINY1X)
    object EIJMP : Instruction(opcode = 0x9419, InstructionOptions.DF_NO_EIJMP)
    object ICALL : Instruction(opcode = 0x9509, InstructionOptions.DF_TINY1X)
    object EICALL : Instruction(opcode = 0x9519, InstructionOptions.DF_NO_EICALL)
    object RET : Instruction(opcode = 0x9508)
    object RETI : Instruction(opcode = 0x9518)
    object SPM : Instruction(opcode = 0x95e8, InstructionOptions.DF_NO_SPM)
    object ESPM : Instruction(opcode = 0x95f8, InstructionOptions.DF_NO_ESPM)
    object BREAK : Instruction(opcode = 0x9598, InstructionOptions.DF_NO_BREAK)
    object LPM : Instruction(opcode = 0x95c8, InstructionOptions.DF_NO_LPM)
    object ELPM : Instruction(opcode = 0x95d8, InstructionOptions.DF_NO_ELPM)

    data class BSET(val s: Const8) : /* 1001 0100 0sss  */
        Instruction(0x9408 or (s.value shl 4))

    data class BCLR(val s: Const8) : /* 1001 0100 0sss  */
        Instruction(0x9488 or (s.value shl 4))

    data class SER(val d: Reg32) : Instruction(0xef0f or (d.reg shl 4))

    data class COM(val d: Reg32) : Instruction(0x9400 or (d.reg shl 4))
    data class NEG(val d: Reg32) : Instruction(0x9401 or (d.reg shl 4))
    data class INC(val d: Reg32) : Instruction(0x9403 or (d.reg shl 4))
    data class DEC(val d: Reg32) : Instruction(0x940a or (d.reg shl 4))
    data class LSR(val d: Reg32) : Instruction(0x9406 or (d.reg shl 4))
    data class ROR(val d: Reg32) : Instruction(0x9407 or (d.reg shl 4))
    data class ASR(val d: Reg32) : Instruction(0x9405 or (d.reg shl 4))
    data class SWAP(val d: Reg32) : Instruction(0x9402 or (d.reg shl 4))
    data class PUSH(val d: Reg32) : Instruction(0x920f or (d.reg shl 4))
    data class POP(val d: Reg32) : Instruction(0x900f or (d.reg shl 4))


    data class TST(val d: Reg32) : Instruction(0x2000 or (d.reg shl 5) or d.reg) // AND d d
    data class CLR(val d: Reg32) : Instruction(0x2400 or (d.reg shl 5) or d.reg) // EOR d d
    data class LSL(val d: Reg32) : Instruction(0x0c00 or (d.reg shl 5) or d.reg) // ADD d d
    data class ROL(val d: Reg32) : Instruction(0x1c00 or (d.reg shl 5) or d.reg) // ADC d d


    /*
        {"breq",  0xf001,          0},
        {"brne",  0xf401,          0},
        {"brcs",  0xf000,          0},
        {"brcc",  0xf400,          0},
        {"brsh",  0xf400,          0},
        {"brlo",  0xf000,          0},
        {"brmi",  0xf002,          0},
        {"brpl",  0xf402,          0},
        {"brge",  0xf404,          0},
        {"brlt",  0xf004,          0},
        {"brhs",  0xf005,          0},
        {"brhc",  0xf405,          0},
        {"brts",  0xf006,          0},
        {"brtc",  0xf406,          0},
        {"brvs",  0xf003,          0},
        {"brvc",  0xf403,          0},
        {"brie",  0xf007,          0},
        {"brid",  0xf407,          0},
        {"rjmp",  0xc000,          0},
        {"rcall", 0xd000,          0},
     */


    data class ADC(val d: Reg32, val r: Reg32) : /* Rd, Rr   0001 11rd dddd rrrr */
        Instruction(0x1c00 or ((r.reg shl 6) or r.reg) and 0b1000001111 or (d.reg shl 4))

}

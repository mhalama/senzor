package cz.zorn.iruploader

interface IRMessageSender {
    suspend fun sendMessage(message: String)
}

class IRMessageSenderImpl(private val transmitter: IRTransmitter) : IRMessageSender {
    override suspend fun sendMessage(message: String) {
        val pattern = mutableListOf<Int>()

        message.forEach { c ->
            val b = c.code
            for (i in 0..7) {
                pattern.add(PROTO_BIT_START)
                if ((b shr i) and 1 == 1) {
                    pattern.add(PROTO_BIT_1)
                } else {
                    pattern.add(PROTO_BIT_0)
                }
            }
        }

        pattern.add(PROTO_END_MARK)
        pattern.add(PROTO_END_DONE)

        transmitter.transmit(IR_FREQUENCY, pattern.toIntArray())
    }

    companion object {
        const val IR_FREQUENCY = 38000

        const val PULSE_LENGTH_US = 400
        const val PROTO_END_MARK = PULSE_LENGTH_US * 3
        const val PROTO_END_DONE = PULSE_LENGTH_US
        const val PROTO_BIT_START = PULSE_LENGTH_US
        const val PROTO_BIT_0 = PULSE_LENGTH_US
        const val PROTO_BIT_1 = PULSE_LENGTH_US * 4
    }
}
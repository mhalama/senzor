package cz.zorn.iruploader

import kotlinx.coroutines.delay
import timber.log.Timber
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer

fun interface IRTransmitter {
    fun transmit(freq: Int, pattern: IntArray)
}

interface IRFirmwareSender {
    suspend fun sendFlash(flash: Map<Int, ByteArray>)
}

class IRFirmwareSenderImpl(private val transmitter: IRTransmitter) : IRFirmwareSender {
    var nimbble_symbols = arrayOf<Byte>(
        0xd, 0xe, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
        0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
    )

    private fun nibblify(src: ByteArray, dest: ByteArrayOutputStream) {
        for (i in 0 until src.size step 2) {
            val byte = src[i].toUByte().toInt()
            val a = nimbble_symbols[byte shr 4].toInt()
            val b = nimbble_symbols[byte and 0x0F].toInt()
            var c = 0
            var d = 0
            if (i + 1 < src.size) {
                val byte = src[i + 1].toUByte().toInt()
                c = nimbble_symbols[byte shr 4].toInt()
                d = nimbble_symbols[byte and 0x0f].toInt()
            }

            val x = ((a shl 2) or (b shr 4)) and 0xFF
            val y = ((b shl 4) or (c shr 2)) and 0xFF
            val z = ((c shl 6) or d) and 0xFF

            val xyz = byteArrayOf(x.toByte(), y.toByte(), z.toByte())
            dest.write(xyz)
        }
    }

    fun calcCrc(crcInput: UByte, data: UByte): UByte {
        var crc = crcInput xor data
        repeat(8) {
            crc = if ((crc and 0x01u) != 0u.toUByte()) {
                (crc.toInt() shr 1).toUByte() xor 0x8Cu.toUByte()
            } else {
                (crc.toInt() shr 1).toUByte()
            }
        }
        return crc
    }

    private fun encodeByteToIrPattern(bytes: ByteArray): IntArray {
        val buffer = ByteBuffer.wrap(bytes)
        val bits = mutableListOf<Int>()

        var bitCnt = 0 // zustava 0 nez prijde prvni 0
        var lastBit = 0

        while (buffer.hasRemaining()) {
            val b = buffer.get().toUByte().toInt() and 0xFF  // unsigned byte

            for (i in 7 downTo 0) {  // od MSB k LSB
                val bit = (b shr i) and 1

                if (bit == lastBit) bitCnt++
                else if (bitCnt > 0) { // pocatecni 0 ignorujeme
                    bits.add(BIT_DELAY * bitCnt)
                    lastBit = bit
                    bitCnt = 1
                }
            }
        }

        if (bitCnt > 0) {
            bits.add(BIT_DELAY * bitCnt)
        }

        Timber.d(bits.toString())

        return bits.toIntArray()
    }

    override suspend fun sendFlash(flash: Map<Int, ByteArray>) {
        flash.keys.sorted().forEachIndexed { pageIndex, pageAddress ->
            Timber.d("PAGE $pageIndex: $pageAddress")

            val packet = ByteArray(PAGE_SIZE + 5)
            var crc = 0.toUByte()

            packet[0] = (pageAddress shr 8).toByte()
            crc = calcCrc(crc, packet[0].toUByte())
            packet[1] = (pageAddress and 0xFF).toByte()
            crc = calcCrc(crc, packet[1].toUByte())
            packet[2] = (flash.size - pageIndex).toByte()
            crc = calcCrc(crc, packet[2].toUByte())

            flash[pageAddress]?.let { page ->
                for (i in 0 until PAGE_SIZE) {
                    val byte = if (i < page.size) page[i].toUByte() else 0.toUByte()
                    packet[i + 3] = byte.toByte()
                    crc = calcCrc(crc, byte)
                }

                packet[PAGE_SIZE + 3] = crc.toByte()

                val output = ByteArrayOutputStream()
                output.write(ByteArray(14) { 0xCC.toByte() }) // synchronizace 0101010...

                // start symbol pro bootloader
                output.write(
                    byteArrayOf(
                        'F'.code.toByte(),
                        'U'.code.toByte()
                    )
                )

                val packetStr = packet.joinToString(" ") { it.toUByte().toInt().toString(16) }
                Timber.d("PACKET: $packetStr")

                packet[PAGE_SIZE + 4] = 0 // jeden zbytecny znak navic pro jistotu

                nibblify(packet, output)

                val bytez =
                    output.toByteArray().joinToString(" ") { it.toUByte().toInt().toString(16) }
                Timber.d("BYTES TO SEND: $bytez")
                val pattern = encodeByteToIrPattern(output.toByteArray())
                transmitter.transmit(IR_FREQUENCY, pattern)
                delay(300) // pockame az zapise do Flash
            }
        }
    }

    companion object {
        const val IR_FREQUENCY = 38000
        const val PAGE_SIZE = 64
        const val BIT_DELAY = 490
    }
}
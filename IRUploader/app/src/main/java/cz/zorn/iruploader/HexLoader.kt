package cz.zorn.iruploader

import java.io.InputStream
import java.nio.ByteBuffer

interface HexLoader {
    fun loadFlashPages(hexFile: InputStream, pageCount: Int, pageSize: Int): Map<Int, ByteArray>
}

class HexLoaderImpl : HexLoader {
    override fun loadFlashPages(
        hexFile: InputStream,
        pageCount: Int,
        pageSize: Int
    ): Map<Int, ByteArray> {
        val flashSize = pageCount * pageSize
        val data = ByteArray(flashSize) { 0xFF.toByte() }
        val flash = ByteBuffer.wrap(data)

        hexFile.bufferedReader().readLines().forEach { line ->
            if (line.isNotEmpty() && line.startsWith(":")) {
                val length = Integer.parseInt(line.substring(1, 3), 16)
                val startAddress = Integer.parseInt(line.substring(3, 7), 16)
                val recordType = Integer.parseInt(line.substring(7, 9), 16)
                if (recordType == 0) {
                    for (i in 0 until length) {
                        val byteValue =
                            Integer.parseInt(line.substring(9 + i * 2, 11 + i * 2), 16)
                        flash.put(startAddress + i, byteValue.toByte())
                    }
                }
            }
        }

        flash.position(0)

        val flashPages = HashMap<Int, ByteArray>()
        while (flash.hasRemaining()) {
            val flashPage = ByteArray(pageSize)
            val address = flash.position()
            flash.get(flashPage)
            if (flashPage.any { it.toUByte().toInt() != 0xFF }) // stranka neni prazdna
                flashPages.put(address, flashPage)
        }

        return flashPages
    }
}
package cz.zorn.iruploader.parser

import android.R.attr.label

sealed class AsmToken {
    data class Number(val value: Int) : AsmToken()
    data class Identifier(val value: String) : AsmToken()
    object Colon : AsmToken()
    object Comma : AsmToken()
}

fun tokenize(line: String): List<AsmToken> {
    val c = line[0]
    when {
        c.isDigit() -> {
            var i = 1
            while (i < line.length && line[i].isDigit()) {
                i++
            }
            val token = AsmToken.Number(line.substring(0, i).toInt())
            return listOf(token) + tokenize(line.substring(i))
        }

        c.isJavaIdentifierStart() -> {
            var i = 1
            while (i < line.length && line[i].isJavaIdentifierPart()) {
                i++
            }
            val token = AsmToken.Identifier(line.substring(0, i))
            return listOf(token) + tokenize(line.substring(i))
        }

        c == ',' -> {
            return listOf(AsmToken.Colon) + tokenize(line.substring(1))
        }

        c == ';' -> { // Všechno za středníkem je komentář
            return emptyList()
        }

        c.isWhitespace() -> {
            return tokenize(line.substring(1))
        }
    }

    return emptyList()
}

fun parse(tokens: List<AsmToken>) {
    var i = 0
    var label: AsmToken.Identifier? = null

    if (tokens[0] is AsmToken.Identifier && tokens[1] is AsmToken.Colon) {
        //label = tokens[1]
        i = 2
    }

    if (tokens[i] is AsmToken.Identifier) {
        val instruction = tokens[i]
        i++
    } else {
        // jen label nebo prazdna radka

    }
}
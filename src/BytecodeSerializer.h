#ifndef BYTECODE_SERIALIZER_H
#define BYTECODE_SERIALIZER_H

#include "Bytecode.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>

class BytecodeSerializer {
public:
    static void serialize(const std::shared_ptr<BytecodeFunction>& func, const std::vector<std::string>& globalSlotNames, std::ostream& out);
    static std::shared_ptr<BytecodeFunction> deserialize(std::istream& in, std::vector<std::string>& outGlobalSlotNames);

private:
    static void writeString(const std::string& str, std::ostream& out);
    static std::string readString(std::istream& in);

    static void writeChunk(const Chunk& chunk, std::ostream& out);
    static void readChunk(Chunk& chunk, std::istream& in);

    static void writeFunction(const std::shared_ptr<BytecodeFunction>& func, std::ostream& out);
    static std::shared_ptr<BytecodeFunction> readFunction(std::istream& in);
};

#endif // BYTECODE_SERIALIZER_H

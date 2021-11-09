#pragma once

// Simple helper class for serializing and deserializing data

#include <cstdint>
#include <array>


template<std::size_t _size>
class buffer 
{
public:

    // Default initializor will just start at the beginning.
    // TODO: Create initializers for data
    buffer()
    {
        _p = _data.begin();
    }

    ~buffer() = default;
    
    // Just throw a type in here and it will append it to the buffer
    template<class T> int append(T val) 
    {
        v = static_cast<uint64_t>(val);
        for (size_t i = sizeof(T) - 1; i >= 0; i--, _p++)
        {
            *_p = (v >> (i * 8)) & 0xFF; 
        }
        return 0;
        
    };

    template<class T> T get()
    {
        T val{};
        for (size_t i = 0; i < sizeof(T); i++, _p++)
        {
            val |=  (*_p << (i * 8)); 
        }
        return val;
    };

private:
    // Keep the data and the index in an actual std::array for safe keeping
    std::array<uint8_t, _size> _data;
    typename std::array<uint8_t, _size>::iterator _p;

};

void buffer_append_int16(uint8_t* buffer, int16_t number, int32_t *index);
void buffer_append_uint16(uint8_t* buffer, uint16_t number, int32_t *index);
void buffer_append_int32(uint8_t* buffer, int32_t number, int32_t *index);
void buffer_append_uint32(uint8_t* buffer, uint32_t number, int32_t *index);
void buffer_append_float16(uint8_t* buffer, float number, float scale, int32_t *index);
void buffer_append_float32(uint8_t* buffer, float number, float scale, int32_t *index);
int16_t buffer_get_int16(const uint8_t *buffer, int32_t *index);
uint16_t buffer_get_uint16(const uint8_t *buffer, int32_t *index);
int32_t buffer_get_int32(const uint8_t *buffer, int32_t *index);
uint32_t buffer_get_uint32(const uint8_t *buffer, int32_t *index);
float buffer_get_float16(const uint8_t *buffer, float scale, int32_t *index);
float buffer_get_float32(const uint8_t *buffer, float scale, int32_t *index);
bool buffer_get_bool(const uint8_t *buffer, int32_t *index);
void buffer_append_bool(uint8_t *buffer,bool value, int32_t *index);

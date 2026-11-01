#include "symbol_table.h"
#include <string.h>

void symbol_offset( void* symbol_table , int64_t offset )
{
    symbol_item_t* symbol_item = (symbol_item_t*)symbol_table;
    while( symbol_item->magic == SYMBOL_TABLE_ITEM_MAGIC )
    {
        symbol_item->symbol = (void*)( (int64_t)(symbol_item->symbol) + offset );
        symbol_item++;
    }
}

size_t symbol_table_size( void* symbol_table )
{
    size_t size = 0;
    symbol_item_t* symbol_item = (symbol_item_t*)symbol_table;
    while( symbol_item->magic == SYMBOL_TABLE_ITEM_MAGIC )
    {
        size += sizeof(symbol_item_t);
        symbol_item++;
    }
    return size;
}

void* symbol_get( void* symbol_table , const char* symbol_name)
{
    symbol_item_t* symbol_item = (symbol_item_t*)symbol_table;
    while( symbol_item->magic == SYMBOL_TABLE_ITEM_MAGIC )
    {
        if( strcmp( symbol_item->name , symbol_name ) == 0 )
        {
            return symbol_item->symbol;
        }
        symbol_item++;
    }
    return NULL;
}

uint32_t symbol_table_num_get( void* symbol_table)
{
    uint32_t count = 0;
    symbol_item_t* symbol_item = (symbol_item_t*)symbol_table;
    while( symbol_item->magic == SYMBOL_TABLE_ITEM_MAGIC )
    {
        count++;
        symbol_item++;
    }
    return count;
}

const char* symbol_get_name( void* symbol_table , uint32_t index )
{
	symbol_item_t * tbl = (symbol_item_t *)symbol_table;
	return tbl[index].name;
}


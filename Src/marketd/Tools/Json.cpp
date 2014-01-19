
#include <nstd/Buffer.h>

#include "Json.h"

class Token
{
public:
  char token;
  Variant value;
};

static bool nextToken(const tchar_t*& data, Token& token)
{
  while(String::isSpace(*data))
    ++data;
  token.token = *data;
  switch(token.token)
  {
  case '\0':
    return true;
  case '{':
  case '}':
  case '[':
  case ']':
  case ',':
  case ':':
    ++data;
    return true;
  case '"':
    {
      ++data;
      String value;
      for(;;)
        switch(*data)
        {
        case 0:
          return false;
        case '\\':
          {
            ++data;
            switch(*data)
            {
            case '"':
            case '\\':
            case '/':
              value.append(*data);
              ++data;
              break;
            case 'b':
              value.append('\b');
              ++data;
              break;
            case 'f':
              value.append('\f');
              ++data;
              break;
            case 'n':
              value.append('\n');
              ++data;
              break;
            case 'r':
              value.append('\r');
              ++data;
              break;
            case 't':
              value.append('\t');
              ++data;
              break;
            case 'u':
              {
                ++data;
                String k;
                k.reserve(4);
                for(int i = 0; i < 4; ++i)
                  if(*data)
                  {
                    k.append(*data);
                    ++data;
                  }
                  else
                    break;
                int_t i;
                if(k.scanf("%x", &i) == 1)
                {
                  k.printf("%lc", (wchar_t)i);
                  value.append(k);
                }
                break;
              }
              break;
            default:
              value.append('\\');
              value.append(*data);
              ++data;
              break;
            }
          }
          break;
        case '"':
          ++data;
          token.value = value;
          return true;
        default:
          value.append(*data);
          ++data;
          break;
        }
    }
    return false;
  case 't':
    if(String::compare(data, "true", 4) == 0)
    {
      data += 4;
      token.value = true;
      return true;
    }
    return false;
  case 'f':
    if(String::compare(data, "false", 5) == 0)
    {
      data += 5;
      token.value = false;
      return true;
    }
    return false;
  case 'n':
    if(String::compare(data, "null", 4) == 0)
    {
      data += 4;
      token.value.clear(); // creates a null variant
      return true;
    }
    return false;
  default:
    token.token = '#';
    if(*data == '-' || String::isDigit(*data))
    {
      String n;
      bool isDouble = false;
      for(;;)
        switch(*data)
        {
        case 'E':
        case 'e':
        case '-':
        case '+':
          n.append(*data);
          ++data;
          break;
        case '.':
          isDouble = true;
          n.append(*data);
          ++data;
          break;
        default:
          if(String::isDigit(*data))
          {
            n.append(*data);
            ++data;
            break;
          }
          goto scanNumber;
        }
    scanNumber:
      if(isDouble)
      {
        token.value = n.toDouble();
        return true;
      }
      else
      {
        int64_t result = n.toInt64();
        int_t resultInt = (int_t)result;
        if((int64_t)resultInt == result)
          token.value = resultInt;
        else
          token.value = result;
        return true;
      }
    }
    return false;
  }
}

static bool_t parseObject(const tchar_t*& data, Token& token, Variant& result);
static bool_t parseValue(const tchar_t*& data, Token& token, Variant& result);
static bool_t parseArray(const tchar_t*& data, Token& token, Variant& result);

static bool_t parseObject(const tchar_t*& data, Token& token, Variant& result)
{
  if(token.token != '{')
    return false;
  if(!nextToken(data, token))
    return false;
  HashMap<String, Variant>& object = result.toMap();
  String key;
  while(token.token != '}')
  {
    if(token.token != '"')
      return false;
    key = token.value.toString();
    if(!nextToken(data, token))
      return false;
    if(token.token != ':')
      return false;
    if(!nextToken(data, token))
      return false;
    if(!parseValue(data, token, object.append(key, Variant())))
      return false;
    if(token.token == '}')
      break;
    if(token.token != ',')
      return false;
    if(!nextToken(data, token))
      return false;
  } 
  if(!nextToken(data, token)) // skip }
    return false;
  return true;
}

static bool_t parseArray(const tchar_t*& data, Token& token, Variant& result)
{
  if(token.token != '[')
    return false;
  if(!nextToken(data, token))
    return false;
  List<Variant>& list = result.toList();
  while(token.token != ']')
  {
    if(!parseValue(data, token, list.append(Variant())))
      return false;
    if(token.token == ']')
      break;
    if(token.token != ',')
      return false;
    if(!nextToken(data, token))
      return false;
  }
  if(!nextToken(data, token)) // skip ]
    return false;
  return true;
}

static bool_t parseValue(const tchar_t*& data, Token& token, Variant& result)
{
  switch(token.token)
  {
  case '"':
  case '#':
  case 't':
  case 'f':
  case 'n':
    {
      result.swap(token.value);
      if(!nextToken(data, token))
        return false;
      return true;
    }
  case '[':
    return parseArray(data, token, result);
  case '{':
    return parseObject(data, token, result);
  }
  return false;
}

bool_t Json::parse(const tchar_t* data, Variant& result)
{
  Token token;
  if(!nextToken(data, token))
    return false;
  if(!parseValue(data, token, result))
    return false;
  return true;
}

bool_t Json::parse(const String& data, Variant& result)
{
  return parse((const tchar_t*)data, result);
}

bool_t Json::parse(const Buffer& data, Variant& result)
{
  return parse((const tchar_t*)(const byte_t*)data, result);
}

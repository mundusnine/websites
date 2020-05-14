#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define MemorySet               memset
#define MemoryCopy              memcpy
#define CalculateCStringLength  strlen
#define CStringToInt            atoi
#define QuickSort               qsort
#define Log(...) { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); }

static int
CharIsAlpha(int c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

static int
CharIsDigit(int c)
{
    return (c >= '0' && c <= '9');
}

static int
CharIsSpace(int c)
{
    return (c <= 32);
}

static int
CharIsSymbol(int c)
{
    return (c == '{' || c == '}' || c == '*' || c == '|' || c == '`');
}

static int
CharIsText(int c)
{
    return (!CharIsSymbol(c) && c != '@');
}

static int
CStringMatchCaseSensitiveN(char *a, char *b, int n)
{
    int matches = 0;
    if(a && b && n)
    {
        matches = 1;
        for(int i = 0; i < n; ++i)
        {
            if(a[i] != b[i])
            {
                matches = 0;
                break;
            }
        }
    }
    return matches;
}

static int
CStringMatchCaseInsensitive(char *a, char *b)
{
    int matches = 0;
    if(a && b)
    {
        matches = 1;
        for(int i = 0;; ++i)
        {
            if(a[i] != b[i])
            {
                matches = 0;
                break;
            }
            else if(!a[i])
            {
                break;
            }
        }
    }
    return matches;
}

typedef u32 OutputFlags;
#define OutputFlag_HTML      (1<<0)
#define OutputFlag_Markdown  (1<<1)
#define OutputFlag_BBCode    (1<<2)

typedef enum InputType
{
    InputType_RXW,
    InputType_HTML,
}
InputType;

typedef enum PageNodeType
{
    PageNodeType_Invalid,
    PageNodeType_PageTitle,
    PageNodeType_Title,
    PageNodeType_SubTitle,
    PageNodeType_Description,
    PageNodeType_Text,
    PageNodeType_ParagraphBreak,
    PageNodeType_UnorderedList,
    PageNodeType_OrderedList,
    PageNodeType_Code,
    PageNodeType_YouTube,
    PageNodeType_Image,
    PageNodeType_ThumbnailImage,
    PageNodeType_Link,
    PageNodeType_FeatureButton,
    PageNodeType_Lister,
    PageNodeType_Date,
}
PageNodeType ;

typedef enum CodeLanguageType
{
    CodeLanguage_None,
    CodeLanguage_C,
}
CodeLanguageType;

typedef enum OrderedListStyle
{
    OrderedListStyle_Numeric,
    OrderedListStyle_Alphabetic,
    OrderedListStyle_RomanNumeral,
}
OrderedListStyle;

typedef u32 TextStyleFlags;
#define TextStyleFlag_Bold       (1<<0)
#define TextStyleFlag_Italics    (1<<1)
#define TextStyleFlag_Underline  (1<<2)
#define TextStyleFlag_Monospace  (1<<3)

typedef struct PageNode PageNode;
struct PageNode
{
    PageNodeType type;
    char *string;
    int string_length;
    PageNode *next;
    TextStyleFlags text_style_flags;
    PageNode *first_parameter;
    
    union
    {
        struct
        {
            PageNode *first_item;
        }
        unordered_list;
        
        struct
        {
            OrderedListStyle order_style;
            PageNode *first_item;
        }
        ordered_list;
        
        struct
        {
            CodeLanguageType language;
        }
        code;
        
        struct
        {
            char *url;
            int url_length;
        }
        link;
        
        struct
        {
            char *image_path;
            int image_path_length;
            char *link;
            int link_length;
        }
        feature_button;
        
        struct
        {
            int year;
            int month;
            int day;
        }
        date;
    };
};

typedef struct Tokenizer Tokenizer;
struct Tokenizer
{
    char *at;
    int line;
    char *file;
    int break_text_by_commas;
};

#define PARSE_CONTEXT_MEMORY_BLOCK_SIZE_DEFAULT 4096
typedef struct ParseContextMemoryBlock ParseContextMemoryBlock;
struct ParseContextMemoryBlock
{
    int size;
    int alloc_position;
    void *memory;
    ParseContextMemoryBlock *next;
};

typedef struct ParseError ParseError;
struct ParseError
{
    char *file;
    int line;
    char *message;
};

typedef struct ParseContext ParseContext;
struct ParseContext
{
    ParseContextMemoryBlock *head;
    ParseContextMemoryBlock *active;
    int error_stack_size;
    int error_stack_size_max;
    ParseError *error_stack;
};

static void *
ParseContextAllocateMemory(ParseContext *context, int size)
{
    void *memory = 0;
    
    ParseContextMemoryBlock *chunk = context->active;
    if(!chunk || chunk->alloc_position + size > chunk->size)
    {
        ParseContextMemoryBlock *old_chunk = chunk;
        int needed_bytes = size < PARSE_CONTEXT_MEMORY_BLOCK_SIZE_DEFAULT ? PARSE_CONTEXT_MEMORY_BLOCK_SIZE_DEFAULT : size;
        chunk = malloc(sizeof(ParseContextMemoryBlock) + needed_bytes);
        chunk->memory = (char *)chunk + sizeof(ParseContextMemoryBlock);
        chunk->size = needed_bytes;
        chunk->alloc_position = 0;
        chunk->next = 0;
        if(old_chunk)
        {
            old_chunk->next = chunk;
        }
        else
        {
            context->head = chunk;
        }
        context->active = chunk;
    }
    
    memory = (char *)chunk->memory + chunk->alloc_position;
    chunk->alloc_position += size;
    return memory;
}

static char *
ParseContextAllocateCStringCopy(ParseContext *context, char *str)
{
    int needed_bytes = CalculateCStringLength(str)+1;
    char *str_copy = ParseContextAllocateMemory(context, needed_bytes);
    MemoryCopy(str_copy, str, needed_bytes);
    return str_copy;
}

static char *
ParseContextAllocateCStringCopyN(ParseContext *context, char *str, int n)
{
    int needed_bytes = n+1;
    char *str_copy = ParseContextAllocateMemory(context, needed_bytes);
    MemoryCopy(str_copy, str, needed_bytes);
    str_copy[n] = 0;
    return str_copy;
}

static PageNode *
ParseContextAllocateNode(ParseContext *context)
{
    PageNode *node = ParseContextAllocateMemory(context, sizeof(PageNode));
    MemorySet(node, 0, sizeof(*node));
    return node;
}

static void
PushParseError(ParseContext *context, Tokenizer *tokenizer, char *format, ...)
{
    if(!context->error_stack)
    {
        context->error_stack_size = 0;
        context->error_stack_size_max = 32;
        context->error_stack = ParseContextAllocateMemory(context, sizeof(ParseError)*context->error_stack_size_max);
    }
    
    if(context->error_stack_size < context->error_stack_size_max)
    {
        va_list args;
        va_start(args, format);
        int needed_bytes = vsnprintf(0, 0, format, args)+1;
        va_end(args);
        
        char *message = ParseContextAllocateMemory(context, needed_bytes);
        
        va_start(args, format);
        vsnprintf(message, needed_bytes, format, args);
        va_end(args);
        
        message[needed_bytes-1] = 0;
        
        ParseError error = {0};
        {
            error.file = tokenizer->file;
            error.line = tokenizer->line;
            error.message = message;
        }
        
        context->error_stack[context->error_stack_size++] = error;
    }
}

typedef enum TokenType
{
    Token_None,
    Token_Text,
    Token_DoubleNewline,
    Token_Symbol,
    Token_Tag,
    Token_StringConstant,
}
TokenType;

typedef struct Token Token;
struct Token
{
    TokenType type;
    char *string;
    int string_length;
    int lines_traversed;
};

static Token
GetNextTokenFromBuffer(Tokenizer *tokenizer)
{
    char *buffer = tokenizer->at;
    Token token = {0};
    
    for(int i = 0; buffer[i]; ++i)
    {
        // NOTE(rjf): Newline
        if(buffer[i] == '\n' && buffer[i+1] == '\n')
        {
            token.type = Token_DoubleNewline;
            token.string = buffer+i;
            token.string_length = 2;
            break;
        }
        // NOTE(rjf): String Constant
        else if(buffer[i] == '"')
        {
            token.type = Token_StringConstant;
            token.string = buffer+i;
            token.string_length = 0;
            int escaped = 1;
            for(; (token.string[token.string_length] != '"' || escaped) && token.string[token.string_length];
                ++token.string_length)
            {
                if(escaped)
                {
                    escaped = 0;
                }
                else
                {
                    if(token.string[token.string_length] == '\\')
                    {
                        escaped = 1;
                    }
                }
            }
            
            ++token.string_length;
            
            break;
        }
        else if(!CharIsSpace(buffer[i]))
        {
            int j = 0;
            
            // NOTE(rjf): Tag
            if(buffer[i] == '@')
            {
                j = i+1;
                for(j=i+1; buffer[j] && !CharIsSpace(buffer[j]) && CharIsAlpha(buffer[j]); ++j);
                token.type = Token_Tag;
            }
            
            // NOTE(rjf): Symbol
            else if(CharIsSymbol(buffer[i]))
            {
                static char *symbolic_blocks_to_break_out[] =
                {
                    "*",
                    "_",
                    "`",
                    "{",
                    "}",
                    "|",
                };
                
                for(j=i+1; buffer[j] && CharIsSymbol(buffer[j]); ++j);
                token.type = Token_Symbol;
                
                for(int k = 0; k < sizeof(symbolic_blocks_to_break_out)/sizeof(symbolic_blocks_to_break_out[0]);
                    ++k)
                {
                    int length_to_compare = CalculateCStringLength(symbolic_blocks_to_break_out[k]);
                    if(CStringMatchCaseSensitiveN(symbolic_blocks_to_break_out[k], buffer+i, length_to_compare))
                    {
                        j = i+length_to_compare;
                        break;
                    }
                }
            }
            
            // NOTE(rjf): Text
            else
            {
                for(j=i+1;
                    buffer[j] &&
                    CharIsText(buffer[j]) &&
                    buffer[j] != '\n' &&
                    (!tokenizer->break_text_by_commas ||
                     buffer[j] != ','); ++j);
                token.type = Token_Text;
                
                // NOTE(rjf): Add skipped whitespace to text node
                for(; i > 0 && CharIsSpace(buffer[i-1]); --i);
            }
            
            if(j != 0)
            {
                token.string = buffer+i;
                token.string_length = j-i;
                break;
            }
        }
    }
    
    for(int i = 0; i < token.string_length; ++i)
    {
        if(token.string[i] == '\n')
        {
            ++token.lines_traversed;
        }
    }
    
    return token;
}

static Token
PeekToken(Tokenizer *tokenizer)
{
    Token token = GetNextTokenFromBuffer(tokenizer);
    return token;
}

static Token
NextToken(Tokenizer *tokenizer)
{
    Token token = GetNextTokenFromBuffer(tokenizer);
    tokenizer->at = token.string + token.string_length;
    tokenizer->line += token.lines_traversed;
    return token;
}

static int
RequireTokenType(Tokenizer *tokenizer, TokenType type, Token *token_ptr)
{
    int match = 0;
    Token token = GetNextTokenFromBuffer(tokenizer);
    if(token.type == type)
    {
        match = 1;
        if(token_ptr)
        {
            *token_ptr = token;
        }
        tokenizer->at = token.string + token.string_length;
        tokenizer->line += token.lines_traversed;
    }
    return match;
}

static int
TokenMatch(Token token, char *string)
{
    return (token.type != Token_None &&
            CStringMatchCaseSensitiveN(token.string, string, token.string_length) &&
            string[token.string_length] == 0);
}

static int
RequireToken(Tokenizer *tokenizer, char *string, Token *token_ptr)
{
    int match = 0;
    Token token = GetNextTokenFromBuffer(tokenizer);
    if(TokenMatch(token, string))
    {
        match = 1;
        if(token_ptr)
        {
            *token_ptr = token;
        }
        tokenizer->at = token.string + token.string_length;
        tokenizer->line += token.lines_traversed;
    }
    return match;
}

static void
SkipToAfterNextComma(Tokenizer *tokenizer)
{
    for(int i = 0; tokenizer->at[i]; ++i)
    {
        if(tokenizer->at[i] == ',')
        {
            tokenizer->at += i+1;
            break;
        }
    }
}

static void
TrimQuotationMarks(char **text, int *text_length)
{
    if(*text[0] == '"')
    {
        *text = *text + 1;
        *text_length -= 2;
    }
}

static int
ParseTextArgument(ParseContext *context, Tokenizer *tokenizer, char **text, int *text_length)
{
    int found_Link = 0;
    Token link_Tag = {0};
    if(RequireTokenType(tokenizer, Token_StringConstant, &link_Tag) ||
       RequireTokenType(tokenizer, Token_Text, &link_Tag))
    {
        *text = link_Tag.string;
        *text_length = link_Tag.string_length;
        if(*text[0] == '"')
        {
            *text = *text + 1;
            *text_length -= 2;
        }
        found_Link = 1;
    }
    return found_Link;
}

static int
ParseLink(ParseContext *context, Tokenizer *tokenizer, char **link, int *link_length)
{
    return ParseTextArgument(context, tokenizer, link, link_length);
}

static PageNode *
ParseText(ParseContext *context, Tokenizer *tokenizer)
{
    PageNode *result = 0;
    
    Token token = PeekToken(tokenizer);
    int text_style_flags = 0;
    
    PageNode **node_store_target = &result;
    
    while(token.type != Token_None)
    {
        Token tag = {0};
        Token symbol = {0};
        Token text = {0};
        
        if(RequireTokenType(tokenizer, Token_Tag, &tag))
        {
            
            if(TokenMatch(tag, "@PageTitle"))
            {
                Token title_Text = {0};
                
                if(RequireToken(tokenizer, "{", 0))
                {
                    if(RequireTokenType(tokenizer, Token_Text, &title_Text) ||
                       RequireTokenType(tokenizer, Token_StringConstant, &title_Text))
                    {
                        PageNode *node = ParseContextAllocateNode(context);
                        node->type = PageNodeType_PageTitle;
                        node->string = title_Text.string;
                        node->string_length = title_Text.string_length;
                        TrimQuotationMarks(&node->string, &node->string_length);
                        node->text_style_flags = text_style_flags;
                        *node_store_target = node;
                        node_store_target = &(*node_store_target)->next;
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "A page title tag expects {<title text>} to follow.");
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@Title"))
            {
                Token title_Text = {0};
                
                if(RequireToken(tokenizer, "{", 0))
                {
                    if(RequireTokenType(tokenizer, Token_Text, &title_Text) ||
                       RequireTokenType(tokenizer, Token_StringConstant, &title_Text))
                    {
                        PageNode *node = ParseContextAllocateNode(context);
                        node->type = PageNodeType_Title;
                        node->string = title_Text.string;
                        node->string_length = title_Text.string_length;
                        TrimQuotationMarks(&node->string, &node->string_length);
                        node->text_style_flags = text_style_flags;
                        *node_store_target = node;
                        node_store_target = &(*node_store_target)->next;
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "A title tag expects {<title text>} to follow.");
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@SubTitle"))
            {
                Token title_Text = {0};
                
                if(RequireToken(tokenizer, "{", 0))
                {
                    if(RequireTokenType(tokenizer, Token_Text, &title_Text) ||
                       RequireTokenType(tokenizer, Token_StringConstant, &title_Text))
                    {
                        PageNode *node = ParseContextAllocateNode(context);
                        node->type = PageNodeType_SubTitle;
                        node->string = title_Text.string;
                        node->string_length = title_Text.string_length;
                        TrimQuotationMarks(&node->string, &node->string_length);
                        node->text_style_flags = text_style_flags;
                        *node_store_target = node;
                        node_store_target = &(*node_store_target)->next;
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "A sub-title tag expects {<sub-title text>} to follow.");
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@Description"))
            {
                Token text = {0};
                
                if(RequireToken(tokenizer, "{", 0))
                {
                    if(RequireTokenType(tokenizer, Token_Text, &text) ||
                       RequireTokenType(tokenizer, Token_StringConstant, &text))
                    {
                        PageNode *node = ParseContextAllocateNode(context);
                        node->type = PageNodeType_Description;
                        node->string = text.string;
                        node->string_length = text.string_length;
                        TrimQuotationMarks(&node->string, &node->string_length);
                        node->text_style_flags = text_style_flags;
                        *node_store_target = node;
                        node_store_target = &(*node_store_target)->next;
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "A description tag expects {<description text>} to follow.");
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@YouTube"))
            {
                if(RequireToken(tokenizer, "{", 0))
                {
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_YouTube;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    if(ParseLink(context, tokenizer, &node->string, &node->string_length))
                    {
                        if(!RequireToken(tokenizer, "}", 0))
                        {
                            PushParseError(context, tokenizer, "Missing '}'.");
                        }
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "A YouTube tag expects {<youtube link>} to follow.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@Image") || TokenMatch(tag, "@ThumbnailImage"))
            {
                if(RequireToken(tokenizer, "{", 0))
                {
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = TokenMatch(tag, "@Image") ? PageNodeType_Image : PageNodeType_ThumbnailImage;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    if(ParseLink(context, tokenizer, &node->string, &node->string_length))
                    {
                        if(!RequireToken(tokenizer, "}", 0))
                        {
                            PushParseError(context, tokenizer, "Missing '}'.");
                        }
                    }
                    else
                    {
                        PushParseError(context, tokenizer, "An image tag expects {<image link>} to follow.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@Code"))
            {
                Token open_bracket = {0};
                if(RequireToken(tokenizer, "{", &open_bracket))
                {
                    char *link = open_bracket.string+1;
                    int link_length = 0;
                    
                    int bracket_stack = 1;
                    for(int i = 0; link[i]; ++i)
                    {
                        if(link[i] == '{')
                        {
                            ++bracket_stack;
                        }
                        else if(link[i] == '}')
                        {
                            --bracket_stack;
                        }
                        
                        if(bracket_stack == 0)
                        {
                            break;
                        }
                        
                        ++link_length;
                    }
                    
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_Code;
                    node->string = link;
                    node->string_length = link_length;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    tokenizer->at = link + link_length;
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Expected } to follow code block.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "A code tag expects {<code>} to follow.");
                }
            }
            
            else if(TokenMatch(tag, "@Link"))
            {
                if(RequireToken(tokenizer, "{", 0))
                {
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_Link;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    if(!ParseTextArgument(context, tokenizer, &node->string, &node->string_length))
                    {
                        PushParseError(context, tokenizer, "A link tag expects {<text>, <link>} to follow.");
                        goto end_Link_parse;
                    }
                    SkipToAfterNextComma(tokenizer);
                    
                    if(!ParseLink(context, tokenizer, &node->link.url, &node->link.url_length))
                    {
                        PushParseError(context, tokenizer, "A link tag expects {<text>, <link>} to follow.");
                        goto end_Link_parse;
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                    
                    end_Link_parse:;
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@FeatureButton"))
            {
                if(RequireToken(tokenizer, "{", 0))
                {
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_FeatureButton;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    if(!ParseTextArgument(context, tokenizer, &node->feature_button.image_path, &node->feature_button.image_path_length))
                    {
                        PushParseError(context, tokenizer, "A feature button tag expects {<image>, <text>, <link>} to follow.");
                        goto end_FeatureButton_parse;
                    }
                    
                    SkipToAfterNextComma(tokenizer);
                    
                    if(!ParseLink(context, tokenizer, &node->string, &node->string_length))
                    {
                        PushParseError(context, tokenizer, "A feature button tag expects {<image>, <text>, <link>} to follow.");
                        goto end_FeatureButton_parse;
                    }
                    
                    SkipToAfterNextComma(tokenizer);
                    
                    if(!ParseLink(context, tokenizer, &node->feature_button.link, &node->feature_button.link_length))
                    {
                        PushParseError(context, tokenizer, "A feature button tag expects {<image>, <text>, <link>} to follow.");
                        goto end_FeatureButton_parse;
                    }
                    
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Missing '}'.");
                    }
                    
                    end_FeatureButton_parse:;
                }
                else
                {
                    PushParseError(context, tokenizer, "Expected '{'.");
                }
            }
            
            else if(TokenMatch(tag, "@Lister"))
            {
                Token open_bracket = {0};
                if(RequireToken(tokenizer, "{", &open_bracket))
                {
                    
                    char *text = 0;
                    int text_length = 0;
                    
                    text = open_bracket.string+1;
                    for(int i = 0; text[i]; ++i)
                    {
                        if(text[i] == '"')
                        {
                            text = text+i+1;
                            break;
                        }
                    }
                    for(text_length = 0; text[text_length] && text[text_length] != '"'; ++text_length);
                    
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_Lister;
                    node->string = text;
                    node->string_length = text_length;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    tokenizer->at = text + text_length+1;
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Expected } to follow lister data.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "A FeatureButton tag expects {<image>,<string>,<link>} to follow.");
                }
            }
            
            else if(TokenMatch(tag, "@Date"))
            {
                Token open_bracket = {0};
                if(RequireToken(tokenizer, "{", &open_bracket))
                {
                    int year = 0;
                    int month = 0;
                    int day = 0;
                    
                    char *str = open_bracket.string+1;
                    
                    for(int i = 0; str[i]; ++i)
                    {
                        if(CharIsDigit(str[i]))
                        {
                            char num_str[32] = {0};
                            int j = 0;
                            for(; str[i+j] && j < sizeof(num_str) && CharIsDigit(str[j]); ++j)
                            {
                                num_str[j] = str[i+j];
                            }
                            year = CStringToInt(num_str);
                            str += i+j+1;
                            break;
                        }
                    }
                    
                    for(int i = 0; str[i]; ++i)
                    {
                        if(CharIsDigit(str[i]))
                        {
                            char num_str[32] = {0};
                            int j = 0;
                            for(; str[i+j] && j < sizeof(num_str) && CharIsDigit(str[j]); ++j)
                            {
                                num_str[j] = str[i+j];
                            }
                            month = CStringToInt(num_str);
                            str += i+j+1;
                            break;
                        }
                    }
                    
                    for(int i = 0; str[i]; ++i)
                    {
                        if(CharIsDigit(str[i]))
                        {
                            char num_str[32] = {0};
                            int j = 0;
                            for(; str[i+j] && j < sizeof(num_str) && CharIsDigit(str[j]); ++j)
                            {
                                num_str[j] = str[i+j];
                            }
                            day = CStringToInt(num_str);
                            str += i+j;
                            break;
                        }
                    }
                    
                    PageNode *node = ParseContextAllocateNode(context);
                    node->type = PageNodeType_Date;
                    node->string = "";
                    node->string_length = 0;
                    node->date.year = year;
                    node->date.month = month;
                    node->date.day = day;
                    node->text_style_flags = text_style_flags;
                    *node_store_target = node;
                    node_store_target = &(*node_store_target)->next;
                    
                    tokenizer->at = str;
                    if(!RequireToken(tokenizer, "}", 0))
                    {
                        PushParseError(context, tokenizer, "Expected '}' to follow date data.");
                    }
                }
                else
                {
                    PushParseError(context, tokenizer, "A FeatureButton tag expects {<image>,<string>,<link>} to follow.");
                }
            }
            
            else
            {
                PushParseError(context, tokenizer, "Malformed tag.");
            }
            
        }
        else if(RequireTokenType(tokenizer, Token_Text, &text))
        {
            PageNode *node = ParseContextAllocateNode(context);
            node->type = PageNodeType_Text;
            node->string = text.string;
            node->string_length = text.string_length;
            node->text_style_flags = text_style_flags;
            *node_store_target = node;
            node_store_target = &(*node_store_target)->next;
        }
        else if(RequireTokenType(tokenizer, Token_Symbol, &symbol))
        {
            if(TokenMatch(symbol, "*"))
            {
                text_style_flags ^= TextStyleFlag_Italics;
            }
            else if(TokenMatch(symbol, "|"))
            {
                text_style_flags ^= TextStyleFlag_Underline;
            }
            else if(TokenMatch(symbol, "`"))
            {
                text_style_flags ^= TextStyleFlag_Monospace;
            }
            else
            {
                PushParseError(context, tokenizer, "Unexpected symbol '%.*s'", symbol.string_length,
                               symbol.string);
            }
        }
        else if(RequireTokenType(tokenizer, Token_DoubleNewline, &text))
        {
            PageNode *node = ParseContextAllocateNode(context);
            node->type = PageNodeType_ParagraphBreak;
            node->string = 0;
            node->string_length = 0;
            node->text_style_flags = text_style_flags;
            *node_store_target = node;
            node_store_target = &(*node_store_target)->next;
        }
        
        token = PeekToken(tokenizer);
        
        if(context->error_stack_size > 0)
        {
            break;
        }
    }
    
    return result;
}

typedef struct FileProcessData FileProcessData;
struct FileProcessData
{
    InputType input_type;
    OutputFlags output_flags;
    char *filename_no_extension;
    char *html_output_path;
    char *md_output_path;
    char *bbcode_output_path;
    char *html_header;
    char *html_footer;
};

typedef struct ProcessedFile ProcessedFile;
struct ProcessedFile
{
    // NOTE(rjf): Page Content Format Root
    PageNode *root;
    char *html_file_contents;
    
    // NOTE(rjf): File Data
    char *filename;
    char *main_title;
    char *description;
    char *url;
    
    // NOTE(rjf): Date
    int date_year;
    int date_month;
    int date_day;
    
    // NOTE(rjf): General Input/Output Data
    InputType input_type;
    OutputFlags output_flags;
    
    // NOTE(rjf): HTML Output
    char *html_header;
    char *html_footer;
    char *html_output_path;
    FILE *html_output_file;
    
    // NOTE(rjf): Other Formats (TODO)
    char *markdown_output_path;
    FILE *markdown_output_file;
    char *bbcode_output_path;
    FILE *bbcode_output_file;
};

static int
ProcessedFileSortFunction(const void *file1_, const void *file2_)
{
    int result = 0;
    
    const ProcessedFile *file1 = file1_;
    const ProcessedFile *file2 = file2_;
    
    if(file1->date_year < file2->date_year)
    {
        result = 1;
    }
    else if(file1->date_year == file2->date_year)
    {
        if(file1->date_month < file2->date_month)
        {
            result = 1;
        }
        else if(file1->date_month == file2->date_month)
        {
            if(file1->date_day < file2->date_day)
            {
                result = 1;
            }
            else if(file1->date_day == file2->date_day)
            {
                result = 0;
            }
            else if(file1->date_day > file2->date_day)
            {
                result = -1;
            }
        }
        else if(file1->date_month > file2->date_month)
        {
            result = -1;
        }
    }
    else if(file1->date_year > file2->date_year)
    {
        result = -1;
    }
    
    return result;
}

static void
OutputHTMLFromPageNodeTreeToFile_(PageNode *node, FILE *file, int follow_next, ProcessedFile *files, int file_count)
{
    int paragraph_active = 0;
    int thumbnails_active = 0;
    int found_Title = 0;
    
    PageNode *date = 0;
    for(date = node; date; date = date->next)
    {
        if(date->type == PageNodeType_Date)
        {
            break;
        }
    }
    
    static char *month_names[] =
    {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December",
    };
    
    PageNode *previous_node = 0;
    
    for(; node; previous_node = node, node = node->next)
    {
        switch(node->type)
        {
            case PageNodeType_Title:
            {
                fprintf(file, "<h1>%.*s</h1>\n", node->string_length, node->string);
                
                if(!found_Title && date)
                {
                    found_Title = 1;
                    fprintf(file, "<h2>%i %s %i</h2>\n",
                            date->date.day, month_names[date->date.month-1], date->date.year);
                }
                
                break;
            }
            case PageNodeType_SubTitle:
            {
                if(previous_node && previous_node->type != PageNodeType_Title)
                {
                    fprintf(file, "<hr><br>\n");
                }
                fprintf(file, "<h2>%.*s</h2>\n", node->string_length, node->string);
                break;
            }
            case PageNodeType_Text:
            {
                if(!paragraph_active)
                {
                    fprintf(file, "<p>");
                    paragraph_active = 1;
                }
                
                if(node->text_style_flags & TextStyleFlag_Bold)
                {
                    fprintf(file, "<strong>");
                }
                if(node->text_style_flags & TextStyleFlag_Underline)
                {
                    fprintf(file, "<u>");
                }
                if(node->text_style_flags & TextStyleFlag_Italics)
                {
                    fprintf(file, "<i>");
                }
                if(node->text_style_flags & TextStyleFlag_Monospace)
                {
                    fprintf(file, "<span class=\"monospace\">");
                }
                fprintf(file, "%.*s", node->string_length, node->string);
                if(node->text_style_flags & TextStyleFlag_Monospace)
                {
                    fprintf(file, "</span>");
                }
                if(node->text_style_flags & TextStyleFlag_Italics)
                {
                    fprintf(file, "</i>");
                }
                if(node->text_style_flags & TextStyleFlag_Underline)
                {
                    fprintf(file, "</u>");
                }
                if(node->text_style_flags & TextStyleFlag_Bold)
                {
                    fprintf(file, "</strong>");
                }
                
                if(!node->next || (node->next->type != PageNodeType_Text &&
                                   node->next->type != PageNodeType_Link))
                {
                    fprintf(file, "</p>");
                    paragraph_active = 0;
                }
                
                break;
            }
            case PageNodeType_ParagraphBreak:
            {
                if(paragraph_active)
                {
                    fprintf(file, "</p>");
                    paragraph_active = 0;
                }
                break;
            }
            case PageNodeType_UnorderedList:
            {
                fprintf(file, "<ul>\n");
                for(PageNode *list_item = node->unordered_list.first_item;
                    list_item; list_item = list_item->next)
                {
                    fprintf(file, "<li>");
                    OutputHTMLFromPageNodeTreeToFile_(list_item, file, 0, files, file_count);
                    fprintf(file, "</li>");
                }
                fprintf(file, "</ul>\n");
                break;
            }
            case PageNodeType_OrderedList:
            {
                fprintf(file, "<ol>\n");
                for(PageNode *list_item = node->unordered_list.first_item;
                    list_item; list_item = list_item->next)
                {
                    fprintf(file, "<li>");
                    OutputHTMLFromPageNodeTreeToFile_(list_item, file, 0, files, file_count);
                    fprintf(file, "</li>");
                }
                fprintf(file, "</ol>\n");
                break;
            }
            case PageNodeType_Code:
            {
                fprintf(file, "<div class=\"code\"><pre>");
                
                enum
                {
                    CODE_TYPE_default,
                    CODE_TYPE_line_comment,
                    CODE_TYPE_block_comment,
                    CODE_TYPE_keyword,
                    CODE_TYPE_constant,
                    CODE_TYPE_Tag,
                };
                
                int code_type = CODE_TYPE_default;
                
                for(int i = 0; i < node->string_length;)
                {
                    int token_length = 1;
                    
                    code_type = CODE_TYPE_default;
                    
                    if(node->string[i] == '/' && node->string[i+1] == '/')
                    {
                        code_type = CODE_TYPE_line_comment;
                        for(;
                            node->string[i+token_length] &&
                            node->string[i+token_length] != '\n';
                            ++token_length);
                        fprintf(file, "<span class=\"code_text\" style=\"color: #8cba53;\">");
                    }
                    else if(node->string[i] == '/' && node->string[i+1] == '*')
                    {
                        code_type = CODE_TYPE_block_comment;
                        int nest_level = 1;
                        for(; node->string[i+token_length]; ++token_length)
                        {
                            if(node->string[i+token_length] == '*' &&
                               node->string[i+token_length+1] == '/')
                            {
                                nest_level -= 1;
                                if(nest_level <= 0)
                                {
                                    break;
                                }
                            }
                            else if(node->string[i+token_length] == '/' &&
                                    node->string[i+token_length+1] == '*')
                            {
                                nest_level += 1;
                            }
                        }
                        token_length += 2;
                        fprintf(file, "<span class=\"code_text\" style=\"color: #8cba53;\">");
                    }
                    else if(CharIsAlpha(node->string[i]) || node->string[i] == '_')
                    {
                        for(;
                            node->string[i+token_length] &&
                            (node->string[i+token_length] == '_' ||
                             CharIsAlpha(node->string[i+token_length]) ||
                             CharIsDigit(node->string[i+token_length]));
                            ++token_length);
                        
                        static char *keywords[] =
                        {
                            "auto", "break", "case", "char",
                            "const", "continue", "default", "do", "double",
                            "else", "enum", "extern", "float",
                            "for", "goto", "if", "int", "long", "register", "return",
                            "short", "signed", "sizeof", "static",
                            "struct", "switch", "typedef", "union", "unsigned",
                            "void", "volatile", "while",
                        };
                        
                        for(int k = 0; k < sizeof(keywords)/sizeof(keywords[0]); ++k)
                        {
                            if(CStringMatchCaseSensitiveN(node->string+i, keywords[k], token_length) &&
                               token_length == CalculateCStringLength(keywords[k]))
                            {
                                code_type = CODE_TYPE_keyword;
                                fprintf(file, "<span class=\"code_text\" style=\"color: #f4b642;\">");
                                break;
                            }
                        }
                    }
                    else if(CharIsDigit(node->string[i]))
                    {
                        for(;
                            node->string[i+token_length] &&
                            (node->string[i+token_length] == '.' ||
                             CharIsAlpha(node->string[i+token_length]) ||
                             CharIsDigit(node->string[i+token_length]));
                            ++token_length);
                        
                        code_type = CODE_TYPE_constant;
                        fprintf(file, "<span class=\"code_text\" style=\"color: #82c4e5;\">");
                    }
                    else if(node->string[i] == '"')
                    {
                        for(;
                            node->string[i+token_length] &&
                            node->string[i+token_length] != '"';
                            ++token_length);
                        
                        ++token_length;
                        
                        code_type = CODE_TYPE_constant;
                        fprintf(file, "<span class=\"code_text\" style=\"color: #82c4e5;\">");
                    }
                    else if(node->string[i] == '\'')
                    {
                        for(;
                            node->string[i+token_length] &&
                            node->string[i+token_length] != '\'';
                            ++token_length);
                        
                        ++token_length;
                        
                        code_type = CODE_TYPE_constant;
                        fprintf(file, "<span class=\"code_text\" style=\"color: #82c4e5;\">");
                    }
                    else if(node->string[i] == '@')
                    {
                        int nest_level = 0;
                        for(; node->string[i+token_length]; ++token_length)
                        {
                            if(node->string[i+token_length] == '(')
                            {
                                nest_level += 1;
                            }
                            else if(node->string[i+token_length] == ')')
                            {
                                nest_level -= 1;
                                if(nest_level <= 0)
                                {
                                    break;
                                }
                            }
                            else if(node->string[i+token_length] <= 32)
                            {
                                if(nest_level == 0)
                                {
                                    break;
                                }
                            }
                        }
                        
                        ++token_length;
                        
                        code_type = CODE_TYPE_Tag;
                        fprintf(file, "<span class=\"code_text\" style=\"color: #d86312;\">");
                    }
                    
                    for(int j = i; j < i + token_length; ++j)
                    {
                        if(node->string[j] == '<')
                        {
                            fprintf(file, "&lt;");
                        }
                        else if(node->string[j] == '>')
                        {
                            fprintf(file, "&gt;");
                        }
                        else if(node->string[j] == '&')
                        {
                            fprintf(file, "&amp;");
                        }
                        else
                        {
                            fprintf(file, "%c", node->string[j]);
                        }
                    }
                    
                    i += token_length;
                    
                    if(code_type != CODE_TYPE_default)
                    {
                        fprintf(file, "</span>");
                    }
                }
                
                fprintf(file, "</pre></div>");
                
                break;
            }
            case PageNodeType_YouTube:
            {
                fprintf(file, "<div class=\"youtube\"><iframe width=\"100%%\" height=\"315\" src=\"");
                
                for(int i = 0; i < node->string_length; ++i)
                {
                    int length = CalculateCStringLength("watch?v=");
                    if(CStringMatchCaseSensitiveN(node->string+i, "watch?v=", length))
                    {
                        fprintf(file, "embed/");
                        i += length-1;
                    }
                    else
                    {
                        fprintf(file, "%c", node->string[i]);
                    }
                }
                
                fprintf(file, "\" frameborder=\"0\" allow=\"accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture\" allowfullscreen></iframe></div>");
                break;
            }
            case PageNodeType_Image:
            {
                fprintf(file, "<div class=\"image_container\"><img class=\"image\" src=\"%.*s\"></div>\n", node->string_length, node->string);
                break;
            }
            case PageNodeType_ThumbnailImage:
            {
                if(!thumbnails_active)
                {
                    thumbnails_active = 1;
                    fprintf(file, "<div class=\"thumbnail_image_container\">");
                }
                fprintf(file, "<a href=\"%.*s\"><img class=\"thumbnail_image\" src=\"%.*s\"></a>",
                        node->string_length, node->string,
                        node->string_length, node->string);
                if(thumbnails_active && (!node->next || node->next->type != PageNodeType_ThumbnailImage))
                {
                    thumbnails_active = 0;
                    fprintf(file, "</div>\n");
                }
                break;
            }
            case PageNodeType_Link:
            {
                if(paragraph_active)
                {
                    fprintf(file, "<a class=\"link\" href=\"%.*s\">%.*s</a>",
                            node->link.url_length, node->link.url,
                            node->string_length, node->string);
                }
                else
                {
                    fprintf(file, "<div class=\"standalone_link_container\"><a class=\"link\" href=\"%.*s\">%.*s</a></div>",
                            node->link.url_length, node->link.url,
                            node->string_length, node->string);
                }
                
                break;
            }
            case PageNodeType_FeatureButton:
            {
                fprintf(file, "<div class=\"feature_button\">\n");
                fprintf(file, "<a href=\"%.*s\">\n",
                        node->feature_button.link_length, node->feature_button.link);
                
                fprintf(file, "<div class=\"feature_button_image\" style=\"background-image: url('%.*s');\"></div>\n",
                        node->feature_button.image_path_length, node->feature_button.image_path);
                
                fprintf(file, "<div class=\"feature_button_text\">\n");
                fprintf(file, "%.*s\n", node->string_length, node->string);
                fprintf(file, "</div>\n");
                
                fprintf(file, "</a>\n");
                fprintf(file, "</div>\n");
                break;
            }
            case PageNodeType_Lister:
            {
                char *prefix = node->string;
                int prefix_length = node->string_length;
                
                for(int i = 0; i < file_count; ++i)
                {
                    if(CStringMatchCaseSensitiveN(files[i].filename, prefix, prefix_length))
                    {
                        char *path = files[i].html_output_path;
                        for(int j = 0; path[j]; ++j)
                        {
                            if(CStringMatchCaseSensitiveN(path+j, "generated/", 10))
                            {
                                path += 10;
                                break;
                            }
                        }
                        fprintf(file, "<a class=\"lister_link\" href=\"%s\">(%i/%i/%i) %s</a>\n", path,
                                files[i].date_year, files[i].date_month, files[i].date_day, files[i].main_title);
                    }
                }
                
                break;
            }
            default: break;
        }
    }
}

static void
OutputHTMLFromPageNodeTreeToFile(PageNode *node, FILE *file, ProcessedFile *files, int file_count)
{
    OutputHTMLFromPageNodeTreeToFile_(node, file, 1, files, file_count);
}

typedef struct SiteInfo SiteInfo;
struct SiteInfo
{
    char *canonical_url;
    char *main_title;
    char *author;
    char *twitter_handle;
    char* icon_path;
};

static void
OutputHTMLHeader(SiteInfo *site_info, ProcessedFile *page)
{
    FILE *file = page->html_output_file;
    fprintf(file, "<!DOCTYPE html>\n");
    fprintf(file, "<html lang=\"en\">\n");
    fprintf(file, "<head>\n");
    fprintf(file, "<meta charset=\"utf-8\">");
    // NOTE(bvisness): Consider adding mobile-friendly styles and then adding this line
    fprintf(file, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    fprintf(file, "<meta name=\"author\" content=\"%s\">\n", site_info->author);
    fprintf(file, "<title>%s%s%s</title>\n", page->main_title, site_info->main_title ? " | " : "", site_info->main_title ? site_info->main_title : "");
    fprintf(file, "<meta property=\"og:title\" content=\"%s\">\n", page->main_title);
    fprintf(file, "<meta name=\"twitter:title\" content=\"%s\">\n", page->main_title);
    if (page->description)
    {
        fprintf(file, "<meta name=\"description\" content=\"%s\">\n", page->description);
        fprintf(file, "<meta property=\"og:description\" content=\"%s\">\n", page->description);
        fprintf(file, "<meta name=\"twitter:description\" content=\"%s\">\n", page->description);
    }
    fprintf(file, "<link rel=\"canonical\" href=\"%s/%s\">\n", site_info->canonical_url, page->url);
    fprintf(file, "<meta property=\"og:type\" content=\"website\">\n");
    fprintf(file, "<meta property=\"og:url\" content=\"%s/%s\">\n", site_info->canonical_url, page->url);
    fprintf(file, "<meta property=\"og:site_name\" content=\"%s\">\n", site_info->main_title ? site_info->main_title : "");
    fprintf(file, "<meta name=\"twitter:card\" content=\"summary\">\n");
    fprintf(file, "<meta name=\"twitter:site\" content=\"%s\">\n", site_info->twitter_handle ? site_info->twitter_handle : "");
    fprintf(file, "<link rel=\"stylesheet\" type=\"text/css\" href=\"data/styles.css\">\n");
    if(site_info->icon_path){
        fprintf(file,"<link rel=\"icon\" type=\"image/png\" href=\"%s\">",site_info->icon_path);
    }
    fprintf(file, "</head>\n");
    fprintf(file, "<body>\n");
    if(page->html_header)
    {
        fprintf(file, "%s", page->html_header);
    }
    fprintf(file, "<div class=\"page_content\">\n");
}

static void
OutputHTMLFooter(SiteInfo *site_info, ProcessedFile *page)
{
    fprintf(page->html_output_file, "</div>\n");
    if(page->html_footer)
    {
        fprintf(page->html_output_file, "%s", page->html_footer);
    }
    fprintf(page->html_output_file, "</body>\n");
    fprintf(page->html_output_file, "</html>\n");
}

static ProcessedFile
ProcessFile(char *filename, char *file, FileProcessData *process_data, ParseContext *context)
{
    ProcessedFile processed_file = {0};
    processed_file.filename = filename;
    processed_file.output_flags = process_data->output_flags;
    processed_file.html_header = process_data->html_header;
    processed_file.html_footer = process_data->html_footer;
    
    if(process_data->input_type == InputType_HTML)
    {
        processed_file.html_file_contents = file;
    }
    else if(process_data->input_type == InputType_RXW)
    {
        Tokenizer tokenizer_ = {0};
        Tokenizer *tokenizer = &tokenizer_;
        tokenizer->at = file;
        tokenizer->line = 1;
        tokenizer->file = filename;
        
        PageNode *page = ParseText(context, tokenizer);
        processed_file.root = page;
        
        if(page)
        {
            for(PageNode *node = page; node; node = node->next)
            {
                if(node->type == PageNodeType_PageTitle)
                {
                    processed_file.main_title = ParseContextAllocateCStringCopyN(context, node->string, node->string_length);
                    break;
                }
            }
            
            // NOTE(rjf): If a PageTitle has not been specified, then try to grab the first title and use it.
            if(processed_file.main_title == 0)
            {
                processed_file.main_title = "";
                for(PageNode *node = page; node; node = node->next)
                {
                    if(node->type == PageNodeType_Title)
                    {
                        processed_file.main_title = ParseContextAllocateCStringCopyN(context, node->string, node->string_length);
                        break;
                    }
                }
            }
            
            for(PageNode *node = page; node; node = node->next)
            {
                if(node->type == PageNodeType_Description)
                {
                    processed_file.description = ParseContextAllocateCStringCopyN(context, node->string, node->string_length);
                    break;
                }
            }
            
            for(PageNode *node = page; node; node = node->next)
            {
                if(node->type == PageNodeType_Date)
                {
                    processed_file.date_year = node->date.year;
                    processed_file.date_month = node->date.month;
                    processed_file.date_day = node->date.day;
                    break;
                }
            }
        }
    }
    
    processed_file.url = ParseContextAllocateCStringCopy(context, process_data->filename_no_extension);
    
    if(process_data->output_flags & OutputFlag_HTML)
    {
        processed_file.html_output_path = ParseContextAllocateCStringCopy(context, process_data->html_output_path);
        processed_file.html_output_file = fopen(process_data->html_output_path, "wb");
    }
    
    if(process_data->output_flags & OutputFlag_Markdown)
    {
        processed_file.markdown_output_path = ParseContextAllocateCStringCopy(context, process_data->md_output_path);
        processed_file.markdown_output_file = fopen(process_data->md_output_path, "wb");
    }
    
    if(process_data->output_flags & OutputFlag_BBCode)
    {
        processed_file.bbcode_output_path = ParseContextAllocateCStringCopy(context, process_data->bbcode_output_path);
        processed_file.bbcode_output_file = fopen(process_data->bbcode_output_path, "wb");
    }
    
    return processed_file;
}

static char *
LoadEntireFileAndNullTerminate(char *filename)
{
    char *result = 0;
    FILE *file = fopen(filename, "rb");
    if(file)
    {
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        result = malloc(file_size+1);
        if(result)
        {
            fread(result, 1, file_size, file);
            result[file_size] = 0;
        }
    }
    return result;
}

static void
FreeFileData(void *data)
{
    free(data);
}

typedef struct KeywordPrefixTreeNode KeywordPrefixTreeNode;
struct KeywordPrefixTreeNode
{
    char *prefix;
    int prefix_length;
    char *value;
    int value_length;
    KeywordPrefixTreeNode *have_child;
    KeywordPrefixTreeNode *no_have_child;
};

void
InsertKeywordIntoTree(KeywordPrefixTreeNode **tree, ParseContext *context, char *key, int key_length, char *value, int value_length)
{
    KeywordPrefixTreeNode **node_target = tree;
    for(KeywordPrefixTreeNode *node = *tree;;)
    {
        int matching_key_characters = 0;
        
        if(node)
        {
            for(int i = 0; i < node->prefix_length && i < key_length; ++i)
            {
                if(key[i]  == node->prefix[i])
                {
                    ++matching_key_characters;
                }
                else
                {
                    break;
                }
            }
            
            // NOTE(rjf): We have matching characters, so this is either the node we want, or we want
            // to allocate a new node on the "have" child.
            if(matching_key_characters > 0)
            {
                if(node->prefix_length <= 1)
                {
                    node = node->have_child;
                    node_target = &node->have_child;
                }
                else
                {
                    KeywordPrefixTreeNode *new_node = ParseContextAllocateMemory(context, sizeof(*new_node));
                    new_node->have_child = node;
                    new_node->no_have_child = 0;
                    new_node->prefix = key;
                    new_node->prefix_length = matching_key_characters;
                    new_node->value = 0;
                    new_node->value_length = 0;
                    *node_target = new_node;
                    node = new_node->no_have_child;
                    node_target = &new_node->no_have_child;
                }
            }
            
            // NOTE(rjf): We don't have any matching characters, so move to the no-have child.
            else
            {
                node = node->no_have_child;
                node_target = &node->no_have_child;
            }
        }
        else
        {
            KeywordPrefixTreeNode *new_node = ParseContextAllocateMemory(context, sizeof(*new_node));
            new_node->have_child = 0;
            new_node->no_have_child = 0;
            new_node->prefix = key;
            new_node->prefix_length = key_length;
            new_node->value = value;
            new_node->value_length = value_length;
            *node_target = new_node;
            break;
        }
    }
}

int
GetKeywordValueFromTree(KeywordPrefixTreeNode *tree, char *keyword, int keyword_length, char **value_ptr)
{
    int value_length = 0;
    
    for(KeywordPrefixTreeNode *node = tree; node;)
    {
        int matching_key_characters = 0;
        
        for(int i = 0; i < node->prefix_length && i < keyword_length; ++i)
        {
            if(keyword[i] == node->prefix[i])
            {
                ++matching_key_characters;
            }
            else
            {
                break;
            }
        }
        
        if(matching_key_characters == node->prefix_length)
        {
            if(matching_key_characters == keyword_length)
            {
                if(value_ptr)
                {
                    *value_ptr = node->value;
                    value_length = node->value_length;
                    break;
                }
            }
            else
            {
                node = node->have_child;
            }
        }
        else
        {
            node = node->no_have_child;
        }
    }
    
    return value_length;
}

KeywordPrefixTreeNode *
GenerateKeywordPrefixTreeFromFile(ParseContext *context, char *filename)
{
    KeywordPrefixTreeNode *root = 0;
    
    char *file = LoadEntireFileAndNullTerminate(filename);
    
    for(int i = 0; file[i]; ++i)
    {
        char *key = 0;
        int key_length = 0;
        char *value = 0;
        int value_length = 0;
        if(!CharIsSpace(file[i]))
        {
            key = file+i+1;
            for(; key[key_length] != ':'; ++key_length);
            value = file+i+key_length+1;
            for(int j = 0; CharIsSpace(file[i+key_length+j]); ++j, ++value);
            for(; value[value_length] != '\n'; ++value_length);
            InsertKeywordIntoTree(&root, context, key, key_length, value, value_length);
        }
    }
    
    FreeFileData(file);
    
    return root;
}

int
main(int argument_count, char **arguments)
{
    int expected_file_count = 0;
    OutputFlags output_flags = 0;
    char *html_header_path = 0;
    char *html_footer_path = 0;
    char *html_header = "";
    char *html_footer = "";
    SiteInfo site_info = {0};
    
    for(int i = 1; i < argument_count; ++i)
    {
        
        // NOTE(rjf): Non-input arguments (just flags).
        if(CStringMatchCaseInsensitive(arguments[i], "--html"))
        {
            Log("Outputting to HTML.");
            output_flags |= OutputFlag_HTML;
            arguments[i] = 0;
        }
        else if(CStringMatchCaseInsensitive(arguments[i], "--markdown"))
        {
            Log("Outputting to markdown.");
            output_flags |= OutputFlag_Markdown;
            arguments[i] = 0;
        }
        else if(CStringMatchCaseInsensitive(arguments[i], "--bbcode"))
        {
            Log("Outputting to BBcode.");
            output_flags |= OutputFlag_BBCode;
            arguments[i] = 0;
        }
        
        // NOTE(rjf): Arguments with input data (not just flags).
        else if(argument_count > i+1)
        {
            if(CStringMatchCaseInsensitive(arguments[i], "--html_header"))
            {
                html_header_path = arguments[i+1];
                Log("HTML header path set as \"%s\".", html_header_path);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--html_footer"))
            {
                html_footer_path = arguments[i+1];
                Log("HTML footer path set as \"%s\".", html_footer_path);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--main_title"))
            {
                site_info.main_title = arguments[i+1];
                Log("Main title set as \"%s\".", site_info.main_title);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--author"))
            {
                site_info.author = arguments[i+1];
                Log("Author set as \"%s\".", site_info.author);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--twitter_handle"))
            {
                site_info.twitter_handle = arguments[i+1];
                Log("Twitter handle set as \"%s\".", site_info.twitter_handle);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--canonical_url"))
            {
                site_info.canonical_url = arguments[i+1];
                Log("Canonical URL set as \"%s\".", site_info.canonical_url);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
            else if(CStringMatchCaseInsensitive(arguments[i], "--icon")){
                site_info.icon_path = arguments[i+1];
                Log("Favicon path set as \"%s\".", site_info.icon_path);
                arguments[i] = 0;
                arguments[i+1] = 0;
                ++i;
            }
        }
        
        // NOTE(rjf): Just a file to parse.
        else
        {
            ++expected_file_count;
        }
        
    }
    
    if(html_header_path)
    {
        html_header = LoadEntireFileAndNullTerminate(html_header_path);
    }
    
    if(html_footer_path)
    {
        html_footer = LoadEntireFileAndNullTerminate(html_footer_path);
    }
    
    ParseContext context = {0};
    ProcessedFile files[4096];
    int file_count = 0;
    
    for(int i = 1; i < argument_count; ++i)
    {
        char *filename = arguments[i];
        if(filename)
        {
            Log("Processing file \"%s\".", filename);
            
            char *file = LoadEntireFileAndNullTerminate(filename);
            
            char extension[256] = {0};
            char filename_no_extension[256] = {0};
            char html_output_path[256] = {0};
            char md_output_path[256] = {0};
            char bbcode_output_path[256] = {0};
            
            snprintf(filename_no_extension, sizeof(filename_no_extension), "%s", filename);
            char *last_period = filename_no_extension;
            for(int i = 0; filename_no_extension[i]; ++i)
            {
                if(filename_no_extension[i] == '.')
                {
                    last_period = filename_no_extension+i;
                }
            }
            *last_period = 0;
            
            snprintf(html_output_path, sizeof(html_output_path), "generated/%s.html", filename_no_extension);
            snprintf(md_output_path, sizeof(md_output_path), "generated/%s.md", filename_no_extension);
            snprintf(bbcode_output_path, sizeof(bbcode_output_path), "generated/%s.bbcode", filename_no_extension);
            
            snprintf(extension, sizeof(extension), "%s", last_period+1);
            InputType input_type = InputType_RXW;
            if(CStringMatchCaseInsensitive(extension, "html"))
            {
                input_type = InputType_HTML;
            }
            
            FileProcessData process_data = {0};
            {
                process_data.input_type = input_type;
                process_data.output_flags = output_flags;
                process_data.filename_no_extension = filename_no_extension;
                process_data.html_output_path = html_output_path;
                process_data.md_output_path = md_output_path;
                process_data.bbcode_output_path = bbcode_output_path;
                process_data.html_header = html_header;
                process_data.html_footer = html_footer;
            }
            
            ProcessedFile processed_file = ProcessFile(filename, file, &process_data, &context);
            
            if(file_count < sizeof(files)/sizeof(files[0]))
            {
                files[file_count++] = processed_file;
            }
            else
            {
                fprintf(stderr, "ERROR: Max file count reached. @Ryan, increase this.\n");
            }
            
        }
    }
    
    // NOTE(rjf): Print errors.
    if(context.error_stack_size > 0)
    {
        for(int i = 0; i < context.error_stack_size; ++i)
        {
            fprintf(stderr, "Parse Error (%s:%i): %s\n",
                    context.error_stack[i].file,
                    context.error_stack[i].line,
                    context.error_stack[i].message);
        }
    }
    
    // NOTE(rjf): Sort files by date.
    {
        QuickSort(files, file_count, sizeof(ProcessedFile), ProcessedFileSortFunction);
    }
    
    // NOTE(rjf): Generate code for all processed files.
    {
        for(int i = 0; i < file_count; ++i)
        {
            ProcessedFile *file = files+i;
            
            if(file->html_output_file)
            {
                OutputHTMLHeader(&site_info, file);
                if(file->root)
                {
                    OutputHTMLFromPageNodeTreeToFile(file->root, file->html_output_file,
                                                     files, file_count);
                }
                else if(file->html_file_contents)
                {
                    fprintf(file->html_output_file, "%s", file->html_file_contents);
                }
                OutputHTMLFooter(&site_info, file);
            }
            
            if(file->markdown_output_file)
            {
                // TODO(rjf)
            }
            
            if(file->bbcode_output_file)
            {
                // TODO(rjf)
            }
        }
    }
    
    return 0;
}

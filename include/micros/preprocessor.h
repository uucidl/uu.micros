#pragma once

#define CONCAT_INNER(A,B) A##B
#define CONCAT(A,B) CONCAT_INNER(A,B)
#define STRING_INNER(A) #A
#define STRING(A) STRING_INNER(A)

#define IDENTITY(A) A
#define PATH(A, B) IDENTITY(A)IDENTITY(B)

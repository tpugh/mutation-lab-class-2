#include <stdio.h>

int main()
{
    //1
    FILE *file;

    //2
    int totalLinesCount = 0;
    char currentCharacter;

    //3
    file = fopen("C://MyFile.txt", "r");

    //4
    if (file == NULL)
    {
        printf("The file doesn't exist ! Please check again .");
        return 0;
    }

    //5
    while ((currentCharacter = fgetc(file)) != EOF)
    {
        //6
        if (currentCharacter == '\n')
        {
            totalLinesCount++;
        }
    }

    //7
    fclose(file);

    //8
    totalLinesCount++;

    //9
    printf("Total number of lines are : %d\n", totalLinesCount);

    return 0;
}
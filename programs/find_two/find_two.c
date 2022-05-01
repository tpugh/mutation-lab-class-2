#include<stdio.h>

int main()
{
    //1
    int i;
    int firstNumber,secondNumber;

    //2
    int firstDivisorSum = 0;
    int secondDivisorSum = 0;

    //3
    printf("Enter two numbers to check if Amicable or not : ");
    scanf("%d %d",&firstNumber,&secondNumber);

    //4
    for(int i=1;i<firstNumber;i++){
    //5
        if(firstNumber % i == 0){
            firstDivisorSum = firstDivisorSum + i;
        }
    }

    //6
    for(int i=1;i<secondNumber;i++){
        if(secondNumber % i == 0){
            secondDivisorSum = secondDivisorSum + i;
        }
    }

    //7
    if((firstNumber == secondDivisorSum) && (secondNumber == firstDivisorSum)){
        printf("%d and %d are Amicable numbers\n",firstNumber,secondNumber);
    }else{
        printf("%d and %d are not Amicable numbers\n",firstNumber,secondNumber);
    }

}
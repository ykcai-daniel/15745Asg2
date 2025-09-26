int liveness_test(int a, int b){
    int c=a+b;
    int d=a-b;
    c=a;
    int f = 3*c;
    int g = 5*d;
    if(a>3){
        int t = f+1;
        int x = t+1;
        f=a+1;
        return x;
    }
    return f+g;
}
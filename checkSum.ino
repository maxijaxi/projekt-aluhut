
int checksum(string _input) {

}

char* charArr(string _input) {
    char* arr = new char[_input.length() + 1];
    for (int i = 0; i < _input.length(); i++) {
        arr[i] = _input[i];
    }
    arr[_input.length()] = '\0'; // Null-terminate the string
    return arr;
}

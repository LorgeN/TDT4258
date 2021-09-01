def check_palindrome(text):
    length = len(text) // 2
    text = list(text)
    return __check_palindrome(text, 0 , len(text) - 1, length)

def lower(text, i):
    if ord(text[i]) < 97:
        text[i] = chr(ord(text[i]) + 32)

def __check_palindrome(text, i, j, length):
    while ord(text[i]) <= 32:
        i += 1

    while ord(text[j]) <= 32:
        j -= 1

    lower(text, i)
    lower(text, j)
    
    if not text[i] == text[j]:
        return False
    if i >= length:
        return True
    return __check_palindrome(text, i + 1, j - 1, length) 

PALINDROMES = ["level", "8448", "KayAk", "step on no pets", "Never odd or even"]
NOT_PALINDROMES = ["ad8dF90", "e082 2F01"]

if __name__ == "__main__":
    for text in PALINDROMES:
        if not check_palindrome(text):
            raise ValueError(f"Wrong output for {text}! Expected true!")
    
    for text in NOT_PALINDROMES:
        if check_palindrome(text):
            raise ValueError(f"Wrong output for {text}! Expected false!")

    print("All tests succesful!")
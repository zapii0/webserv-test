#!/usr/bin/python3
import os
import sys

# Nagłówki CGI MUSZĄ być oddzielone od ciała odpowiedzi jedną pustą linią (\r\n\r\n)
# Funkcja parseCgiOutput() z Twojego serwera szuka tego podziału.
print("Content-Type: text/html\r\n\r\n", end="")

print("<!DOCTYPE html><html><head><title>Wynik CGI</title></head>")
print("<body style='font-family: Arial; background: #2d2d2d; color: #fff; padding: 20px;'>")

print("<h1 style='color: #4CAF50;'>Skrypt CGI został poprawnie wykonany!</h1>")

print("<h2>Zmienne środowiskowe odczytane przez CGI:</h2><ul>")

# Wypisywanie zmiennych, które przydzielił serwer C++ przed forkiem
target_envs = ["REQUEST_METHOD", "QUERY_STRING", "PATH_INFO", "CONTENT_LENGTH", "CONTENT_TYPE", "SERVER_SOFTWARE"]
for env in target_envs:
    value = os.environ.get(env, "Brak wartości")
    print(f"<li><strong>{env}</strong>: {value}</li>")
print("</ul>")

# Sprawdzanie czy request posiada ciało (np. wysłany przez formularz POST)
method = os.environ.get("REQUEST_METHOD", "")
if method == "POST":
    try:
        content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    except ValueError:
        content_length = 0
        
    if content_length > 0:
        # Odczytywanie danych przesyłanych przez pipe'y (STDIN_FILENO) z Twojego C++
        body = sys.stdin.read(content_length)
        print("<h2>Otrzymane ciało (Body) zapytania POST:</h2>")
        print(f"<pre style='background: #111; padding: 10px;'>{body}</pre>")

print("<br><a href='/' style='color: #4CAF50;'>Wróc do strony testowej</a>")
print("</body></html>")
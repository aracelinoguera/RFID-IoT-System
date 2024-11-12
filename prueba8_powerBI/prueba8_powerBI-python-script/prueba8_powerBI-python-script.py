# Etapa 8: Python Script para Power BI

import gspread
import pandas as pd
from google.oauth2 import service_account

# Define el alcance para acceder a Google Sheets y Google Drive
scope = ["https://spreadsheets.google.com/feeds", "https://www.googleapis.com/auth/drive"]

# Carga las credenciales de la cuenta de servicio
credentials = service_account.Credentials.from_service_account_file("C:/Users/Araceli/Documents/Credenciales/archivo-de-credenciales.json", scopes=scope)

# Autenticación con Google Sheets usando las credenciales
client = gspread.authorize(credentials)

# Abre la hoja de Google Sheets usando su URL o ID
spreadsheet = client.open_by_url('URL o ID')

# Selecciona la hoja que quieres trabajar (ejemplo: 'Sheet1')
sheet = spreadsheet.worksheet('Sheet1')

# Extrae los datos a partir de la celda que contiene la información (todos los datos de la hoja desde esa celda)
data = sheet.get_values('A7:M')  # Cambia según el rango máximo de columnas necesarias

# Convierte los datos en un DataFrame de pandas
headers = data[0]  # Los encabezados están en la primera fila del rango seleccionado (fila 7 en Google Sheets)
values = data[1:]  # Los datos están a partir de la segunda fila

# Crear el DataFrame utilizando los encabezados y valores extraídos
df = pd.DataFrame(values, columns=headers)

# Mostrar el DataFrame para verificar la extracción
print(df)
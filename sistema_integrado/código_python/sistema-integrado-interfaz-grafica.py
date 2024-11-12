# Etapa 10: Desarrollo de Interfaz Gráfica con Python Tkinter

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import time

# Inicialización de la conexión serial
ser = None
conectado = False  # Variable para controlar el estado de la conexión

def limpiar_buffer_serial():
    """Función para limpiar el buffer serial antes de enviar un comando."""
    if ser and ser.is_open:
        ser.reset_input_buffer()

def conectar_desconectar():
    global ser, conectado

    if not conectado:
        # Intentar conectar
        try:
            ser = serial.Serial('COM8', 115200, timeout=1)  # Ajusta el puerto y la velocidad según tu configuración
            actualizar_monitor_estado("Estado de Conexión: Conectado.\n\n")
            btn_conectar.config(text="Desconectar")  # Cambiar el texto del botón
            conectado = True
        except:
            messagebox.showerror("Error", "No se pudo establecer conexión con el sistema.")
    else:
        # Desconectar si ya está conectado
        try:
            if ser and ser.is_open:
                ser.close()
                actualizar_monitor_estado("Estado de Conexión: Desconectado.\n")
                btn_conectar.config(text="Conectar")  # Cambiar el texto del botón
                conectado = False
        except:
            messagebox.showerror("Error", "No se pudo desconectar del sistema.")

def actualizar_monitor_estado(texto):
    """Función para actualizar el monitor de estado, solo lectura."""
    monitor_estado.config(state=tk.NORMAL)  # Habilitar escritura temporalmente
    monitor_estado.insert(tk.END, texto)  # Insertar el texto
    monitor_estado.config(state=tk.DISABLED)  # Volver a deshabilitar escritura
    monitor_estado.see(tk.END)  # Desplazarse al final del texto

def programar_etiqueta():
    if not conectado or ser is None or not ser.is_open:
        messagebox.showwarning("Advertencia", "Primero debes establecer conexión con el sistema.")
        return

    ventana_programar = tk.Toplevel(root)
    ventana_programar.title("Ingrese los datos del Reactivo")

    # Campos de entrada
    etiquetas = ["Producto", "Número", "Marca", "Código", "Presentación", "Lote", "Vencimiento"]
    entradas = {}
    
    for i, etiqueta in enumerate(etiquetas):
        tk.Label(ventana_programar, text=etiqueta).grid(row=i, column=0, padx=10, pady=5, sticky="w")
        entradas[etiqueta] = tk.Entry(ventana_programar, width=40)
        entradas[etiqueta].grid(row=i, column=1, padx=10, pady=5)

    def guardar_datos():
        # Crear una cadena concatenada con los valores de los campos separados por comas
        datos_concatenados = ",".join(entrada.get() for entrada in entradas.values())
        print(f"Datos enviados: {datos_concatenados}")  # Imprimir la cadena completa para verificar
        try:
            limpiar_buffer_serial()  # Limpiar el buffer antes de enviar comandos

            ser.write(b'WRITE\n')  # Envía el comando "WRITE" para iniciar el proceso
            actualizar_monitor_estado("Enviando comando WRITE...\n")
            time.sleep(2)  # Incrementar el tiempo de espera

            # Enviar la cadena concatenada con los datos
            ser.write((datos_concatenados + "\n").encode('utf-8'))
            actualizar_monitor_estado(f"Datos enviados: {datos_concatenados}\n")

            # Leer la respuesta del ESP32 en un bucle hasta recibir el mensaje de éxito o hasta timeout
            respuesta = ""
            timeout = time.time() + 10  # Tiempo límite de 10 segundos
            while time.time() < timeout:
                line = ser.readline().decode('utf-8').strip()
                if line:
                    actualizar_monitor_estado(f"{line}\n")
                    respuesta += line
                if "Datos guardados exitosamente" in line:
                    break  # Salir del bucle cuando llegue la respuesta final

            # Verificar si los datos fueron guardados correctamente
            if "Datos guardados exitosamente" in respuesta:
                messagebox.showinfo("Éxito", "Etiqueta programada correctamente.")
            else:
                messagebox.showerror("Error", "Hubo un problema al programar la etiqueta.")
                
            ventana_programar.destroy()

        except Exception as e:
            messagebox.showerror("Error", f"Error al enviar los datos: {e}")

    # Botón para guardar los datos
    btn_guardar = ttk.Button(ventana_programar, text="Guardar datos", command=guardar_datos)
    btn_guardar.grid(row=len(etiquetas), column=0, columnspan=2, pady=10)

# Función para leer los datos de la etiqueta NFC (Registrar Alta)
def leer_etiqueta():
    if not conectado or ser is None or not ser.is_open:
        messagebox.showwarning("Advertencia", "Primero debes establecer conexión con el sistema.")
        return
    
    try:
        limpiar_buffer_serial()  # Limpiar el buffer antes de enviar comandos

        ser.write(b'READ\n')  # Enviar el comando "READ"
        actualizar_monitor_estado("Enviando comando 'READ'...\n")
        
        time.sleep(2)  # Esperar más tiempo para que el ESP32 responda

        datos_leidos = False  # Variable para verificar si se leen datos
        respuesta_completa = ""  # Acumula toda la respuesta para mostrarla en el monitor

        # Leer las respuestas bloque por bloque
        timeout = time.time() + 10  # Tiempo límite de 10 segundos
        while ser.in_waiting > 0 or time.time() < timeout:
            respuesta = ser.readline().decode('utf-8').strip()
            if respuesta:
                # Acumular toda la respuesta en el monitor de estado
                respuesta_completa += f"{respuesta}\n"
                datos_leidos = True

            # Verificar si la respuesta contiene la confirmación de actualización de fecha y datos en Firebase
            if "Fecha de alta registrada" in respuesta:
                messagebox.showinfo("Actualización", "Fecha de alta registrada con éxito.")
            elif "Lectura completa" in respuesta:
                messagebox.showinfo("Registro completado", "Datos registrados y enviados a Firebase correctamente.")
                actualizar_monitor_estado(f"{respuesta_completa}\n")
                break  # Termina el ciclo si ya se completó la lectura y envío a Firebase

        # Mostrar advertencia si no se leyeron datos
        if not datos_leidos:
            actualizar_monitor_estado("No se leyeron o mostraron datos de la etiqueta.\n")
            messagebox.showwarning("Advertencia", "No se leyeron o mostraron datos de la etiqueta.")

    except Exception as e:
        messagebox.showerror("Error", f"Error al leer la etiqueta: {e}")

# Función para registrar el uso de un reactivo
def registrar_uso():
    if not conectado or ser is None or not ser.is_open:
        messagebox.showwarning("Advertencia", "Primero debes establecer conexión con el sistema.")
        return

    try:
        limpiar_buffer_serial()  # Limpiar el buffer antes de enviar comandos

        ser.write(b'TRACK\n')  # Enviar el comando "TRACK" para registrar el uso
        actualizar_monitor_estado("Enviando comando 'TRACK'...\n")
        
        time.sleep(1.5)  # Esperar para que el ESP32 procese la lectura

        datos_leidos = False  # Variable para verificar si se leen datos
        respuesta_completa = ""  # Acumular todas las respuestas para mostrar en el monitor
        confirmacion_firebase = False  # Verificar si llegó confirmación de Firebase
        peso_detectado = False  # Verificar si se recibió el peso del reactivo


        # Leer las respuestas del ESP32
        timeout = time.time() + 30  # Tiempo límite de 10 segundos
        while time.time() < timeout:
            if ser.in_waiting > 0:
                respuesta = ser.readline().decode('utf-8').strip()
                if respuesta:
                    respuesta_completa += f"{respuesta}\n"  # Acumular toda la respuesta
                    datos_leidos = True

                    # Verificar y capturar el peso solo una vez
                    if "Peso:" in respuesta and not peso_detectado:
                        peso_detectado = True
                    # Verificar y capturar la confirmación de Firebase solo una vez
                    elif "Datos enviados con éxito" in respuesta and not confirmacion_firebase:
                        confirmacion_firebase = True
                        messagebox.showinfo("Registro completado", "Uso del reactivo registrado y enviado a Firebase.")
                        actualizar_monitor_estado(f"{respuesta_completa}\n")  # Mostrar la respuesta en el monitor
                        break  # Termina el ciclo al recibir la confirmación

        # Mensaje en caso de que no se reciban datos o confirmación de Firebase
        if not datos_leidos:
            actualizar_monitor_estado("No se completó el registro de uso.\n")
            messagebox.showwarning("Advertencia", "No se completó el registro de uso.")
        elif not confirmacion_firebase:
            respuesta_completa += "Confirmación de Firebase no recibida.\n"
            actualizar_monitor_estado(f"{respuesta_completa}\n")
            messagebox.showwarning("Advertencia", "Confirmación de Firebase no recibida para el uso del reactivo.")

    except Exception as e:
        messagebox.showerror("Error", f"Error al registrar el uso: {e}")

# Función para registrar la baja de un reactivo
def registrar_baja():
    if not conectado or ser is None or not ser.is_open:
        messagebox.showwarning("Advertencia", "Primero debes establecer conexión con el sistema.")
        return

    try:
        limpiar_buffer_serial()  # Limpiar el buffer antes de enviar comandos

        ser.write(b'OUT\n')  # Enviar el comando "OUT" para registrar la baja
        actualizar_monitor_estado("Enviando comando 'OUT'...\n")
        
        time.sleep(2)  # Esperar para que el ESP32 procese la lectura

        datos_leidos = False  # Verificar si se leen datos
        respuesta_completa = ""  # Acumula toda la respuesta para mostrarla en el monitor

        # Leer las respuestas del ESP32 bloque por bloque
        timeout = time.time() + 10  # Tiempo límite de 10 segundos
        while ser.in_waiting > 0 or time.time() < timeout:
            respuesta = ser.readline().decode('utf-8').strip()
            if respuesta:
                # Acumular toda la respuesta en el monitor de estado
                respuesta_completa += f"{respuesta}\n"
                datos_leidos = True

            # Verificar si la respuesta contiene confirmación de registro de baja en Firebase
            if "Fecha de baja registrada" in respuesta:
                messagebox.showinfo("Actualización", "Fecha de baja registrada con éxito.")
            elif "Lectura completa." in respuesta:
                messagebox.showinfo("Registro completado", "Baja registrada y enviada a Firebase correctamente.")
                actualizar_monitor_estado(f"{respuesta_completa}\n")
                break  # Termina el ciclo si ya se completó el registro de baja y envío a Firebase

        # Mostrar advertencia si no se leyeron datos
        if not datos_leidos:
            actualizar_monitor_estado("No se completó el registro de baja del reactivo.\n")
            messagebox.showwarning("Advertencia", "No se completó el registro de baja del reactivo.")

    except Exception as e:
        messagebox.showerror("Error", f"Error al registrar la baja: {e}")


# Configuración de la ventana principal
root = tk.Tk()
root.title("Sistema de Gestión de Reactivos")
root.geometry("680x540")

# Etiqueta y botón para conectar/desconectar al ESP32
tk.Label(root, text="Establece Conexión con el Sistema").pack(pady=(20, 5))
btn_conectar = ttk.Button(root, text="Conectar", command=conectar_desconectar)
btn_conectar.pack(pady=(0, 10))

# Monitor de estado (solo lectura)
monitor_estado = scrolledtext.ScrolledText(root, height=17, width=70, state=tk.DISABLED)  # Inicialmente en modo solo lectura
monitor_estado.pack(padx=10, pady=10)

# Texto para indicar ingreso de nuevo reactivo
ttk.Label(root, text="Nuevo Reactivo:").pack(pady=(10, 1))

# Botón para programar una nueva etiqueta, centrado
btn_programar = ttk.Button(root, text="Programar Etiqueta", command=programar_etiqueta)
btn_programar.pack(pady=5)

# Texto para indicar selección de opciones
ttk.Label(root, text="Opciones de Registro:").pack(pady=(20, 5))

# Frame para organizar los botones horizontalmente, centrado
frame_botones = tk.Frame(root)
frame_botones.pack()

# Botón para registrar la alta de un reactivo
btn_leer = ttk.Button(frame_botones, text="Registrar Alta", command=leer_etiqueta)
btn_leer.grid(row=0, column=0, padx=5)

# Botón para registrar el uso de un reactivo
btn_registrar_uso = ttk.Button(frame_botones, text="Registrar Uso", command=registrar_uso)
btn_registrar_uso.grid(row=0, column=1, padx=5)

# Botón para registrar la baja de un reactivo
btn_registrar_baja = ttk.Button(frame_botones, text="Registrar Baja", command=registrar_baja)
btn_registrar_baja.grid(row=0, column=2, padx=5)

root.mainloop()


import pandas as pd
import numpy as np
from prophet import Prophet
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.lines as mlines

# Configuración inicial
solventes = ["Metanol", "Hexano", "Éter de petróleo liviano", "Éter de petróleo pesado"]
proveedores = {
    "Metanol": [("Proveedor A", 7), ("Proveedor B", 14)],
    "Hexano": [("Proveedor C", 10), ("Proveedor D", 20)],
    "Éter de petróleo liviano": [("Proveedor E", 8), ("Proveedor F", 12)],
    "Éter de petróleo pesado": [("Proveedor G", 6), ("Proveedor H", 11)]
}
capacidad_contenedor = 1
almacen_inicial = 5
cantidad_pedido = 2
nivel_reorden = 0.5
fechas = pd.date_range(start="2020-01-01", end="2024-10-31", freq='D')

# Generación de datos sintéticos
def consumo_estacional(solvente, mes):
    if solvente == "Metanol":
        return 0.04 if mes in [3, 4, 9, 10] else 0.03
    elif solvente == "Hexano":
        return 0.038 if mes in [6, 7, 12, 1] else 0.028
    elif solvente == "Éter de petróleo liviano":
        return 0.035 if mes in [6, 7, 8] else 0.025
    elif solvente == "Éter de petróleo pesado":
        return 0.037 if mes in [3, 4, 5, 11, 12] else 0.027

datos_diarios = []
for fecha in fechas:
    mes = fecha.month
    ruido_diario = np.random.normal(loc=1, scale=0.1)
    for solvente in solventes:
        consumo_dia_base = consumo_estacional(solvente, mes)
        consumo_dia_total = consumo_dia_base * ruido_diario * np.random.uniform(0.8, 1.2)
        proveedor, lead_time = proveedores[solvente][np.random.randint(0, 2)]
        datos_diarios.append({
            "Fecha": fecha,
            "Solvente": solvente,
            "Consumo Diario (L)": consumo_dia_total,
            "Proveedor": proveedor,
            "Lead Time (días)": lead_time
        })

df_diario = pd.DataFrame(datos_diarios)
df_diario = df_diario.groupby(["Fecha", "Solvente"], as_index=False).agg({
    "Consumo Diario (L)": "sum",
    "Proveedor": "first",
    "Lead Time (días)": "first"
})

def calcular_agotamiento_y_pedidos(df, solvente):
    inventario_actual = almacen_inicial
    consumo_acumulado = 0
    dias_termino_contenedor = []
    pedidos = []

    # Ajustar las columnas a los nombres esperados
    if "Cantidad de uso (L)" in df.columns:
        df = df.rename(columns={"Cantidad de uso (L)": "y"})
    elif "yhat" in df.columns:
        df = df.rename(columns={"yhat": "y"})  # Para predicciones

    fecha_agotamiento = None  # Variable para rastrear la última fecha de agotamiento

    for _, row in df.iterrows():
        consumo_acumulado += row["y"]
        inventario_actual -= row["y"]

        # Registrar fin de contenedor
        if consumo_acumulado >= capacidad_contenedor:
            fecha_agotamiento = row["Fecha de uso"]
            dias_termino_contenedor.append({"Fecha de Agotamiento": fecha_agotamiento, "Solvente": solvente})
            consumo_acumulado = 0

        # Predecir agotamiento y generar pedidos anticipadamente
        if inventario_actual <= nivel_reorden:
            dias_restantes = max(0, int(inventario_actual // row["y"]))
            fecha_predicha_agotamiento = pd.to_datetime(row["Fecha de uso"]) + pd.Timedelta(days=dias_restantes)
            proveedor, lead_time = proveedores[solvente][np.random.randint(0, 2)]
            fecha_pedido = fecha_predicha_agotamiento - pd.Timedelta(days=lead_time)

            pedidos.append({
                "Fecha de Agotamiento": fecha_predicha_agotamiento,
                "Fecha Pedido": fecha_pedido,
                "Fecha Recepción": fecha_predicha_agotamiento,
                "Solvente": solvente,
                "Cantidad Pedido (L)": cantidad_pedido,
                "Proveedor": proveedor,
                "Lead Time (días)": lead_time
            })

            inventario_actual += cantidad_pedido

    df_agotamiento = pd.DataFrame(dias_termino_contenedor)
    df_pedidos = pd.DataFrame(pedidos)

    return df_agotamiento, df_pedidos

def predecir_consumo_y_pedidos(solvente, fecha_inicio="2024-11-01", fecha_fin="2027-12-31"):
    # Preparar los datos históricos
    df_solvente = df_diario[df_diario["Solvente"] == solvente][["Fecha", "Consumo Diario (L)"]].copy()
    df_solvente.rename(columns={"Fecha": "ds", "Consumo Diario (L)": "y"}, inplace=True)
    
    # Ajustar el modelo Prophet
    modelo = Prophet(yearly_seasonality=True, daily_seasonality=False)
    modelo.fit(df_solvente)
    
    # Generar predicciones
    fechas_prediccion = pd.date_range(start=fecha_inicio, end=fecha_fin, freq='D')
    df_prediccion = pd.DataFrame({"ds": fechas_prediccion})
    pronostico = modelo.predict(df_prediccion)
    
    # Gráfico de predicción
    fig, ax = plt.subplots(figsize=(10, 6))
    modelo.plot(pronostico, ax=ax)  # Crear el gráfico original de Prophet
    
    # Configurar títulos en español
    ax.set_title(f"Predicción de Consumo - {solvente}", fontsize=14)
    ax.set_xlabel("Fecha")
    ax.set_ylabel("Consumo (L)")
    
    # Datos históricos y predicciones
    df_historico = df_solvente.rename(columns={"ds": "Fecha de uso", "y": "Cantidad de uso (L)"})
    df_historico["Solvente"] = solvente
    df_prediccion_merged = pronostico[["ds", "yhat"]].rename(columns={"ds": "Fecha de uso", "yhat": "Cantidad de uso (L)"})
    df_prediccion_merged["Solvente"] = solvente
    
    # Calcular fechas de agotamiento y pedidos
    df_dias_termino_historico, df_pedidos_historico = calcular_agotamiento_y_pedidos(df_historico, solvente)
    df_dias_termino_prediccion, df_pedidos_prediccion = calcular_agotamiento_y_pedidos(df_prediccion_merged, solvente)
    
    # Agregar líneas verticales para las fechas de agotamiento
    for fecha_agotamiento in pd.concat([df_dias_termino_historico["Fecha de Agotamiento"], 
                                        df_dias_termino_prediccion["Fecha de Agotamiento"]]).dropna():
        ax.axvline(pd.to_datetime(fecha_agotamiento), color="#AEAEAE", linestyle=":", linewidth=0.8, label="Fecha de Agotamiento")
    
    # Agregar líneas verticales para las fechas de pedidos
    for fecha_pedido in pd.concat([df_pedidos_historico["Fecha Pedido"], 
                                   df_pedidos_prediccion["Fecha Pedido"]]).dropna():
        ax.axvline(pd.to_datetime(fecha_pedido), color="#45918A", linestyle="-.", linewidth=0.8, label="Fecha de Pedido")
    
    # Leyenda personalizada en español
    observed_legend = mlines.Line2D([], [], marker='o', color='black', linestyle='None', markersize=5, label="Datos Históricos")
    forecast_legend = mlines.Line2D([], [], color='#006DAF', lw=2, label="Pronóstico")
    uncertainty_legend = mpatches.Patch(color='lightblue', alpha=0.4, label="Intervalo de Incertidumbre")
    agotamiento_legend = mlines.Line2D([], [], color="#AEAEAE", linestyle=":", linewidth=0.8, label="Fecha de Agotamiento")
    pedido_legend = mlines.Line2D([], [], color="#45918A", linestyle="-.", linewidth=0.8, label="Fecha de Pedido")
    
    ax.legend(handles=[observed_legend, forecast_legend, uncertainty_legend, agotamiento_legend, pedido_legend], 
              loc="center left", bbox_to_anchor=(1, 0.5))

    plt.tight_layout()  # Ajustar para que la leyenda no recorte el gráfico
    plt.show()
    
    return df_historico, df_prediccion_merged, df_pedidos_historico, df_pedidos_prediccion

def guardar_archivos_excel(historico, predicciones, pedidos_historicos, pedidos_predicciones, proveedores):
    hojas_historico = ["consumo_historico_met", "consumo_historico_hex", "consumo_historico_eterliv", "consumo_historico_eterpes"]
    hojas_predicciones = ["prediccion_consumo_met", "prediccion_consumo_hex", "prediccion_consumo_eterliv", "prediccion_consumo_eterpes"]
    hojas_pedidos_hist = ["pedidos_historicos_met", "pedidos_historicos_hex", "pedidos_historicos_eterliv", "pedidos_historicos_eterpes"]
    hojas_pedidos_pred = ["prediccion_pedidos_met", "prediccion_pedidos_hex", "prediccion_pedidos_eterliv", "prediccion_pedidos_eterpes"]

    with pd.ExcelWriter("consumo_historico.xlsx", engine="xlsxwriter") as writer:
        for df, hoja in zip(historico, hojas_historico):
            df["Fecha de uso"] = pd.to_datetime(df["Fecha de uso"]).dt.strftime('%d/%m/%Y')
            df.to_excel(writer, sheet_name=hoja, index=False)

    with pd.ExcelWriter("prediccion_consumo.xlsx", engine="xlsxwriter") as writer:
        for df, hoja in zip(predicciones, hojas_predicciones):
            df["Fecha de uso"] = pd.to_datetime(df["Fecha de uso"]).dt.strftime('%d/%m/%Y')
            df.to_excel(writer, sheet_name=hoja, index=False)

    with pd.ExcelWriter("pedidos_historicos.xlsx", engine="xlsxwriter") as writer:
        for df, hoja in zip(pedidos_historicos, hojas_pedidos_hist):
            df["Fecha de Agotamiento"] = pd.to_datetime(df["Fecha de Agotamiento"]).dt.strftime('%d/%m/%Y')
            df["Fecha Pedido"] = pd.to_datetime(df["Fecha Pedido"]).dt.strftime('%d/%m/%Y')
            df["Fecha Recepción"] = pd.to_datetime(df["Fecha Recepción"]).dt.strftime('%d/%m/%Y')
            df.to_excel(writer, sheet_name=hoja, index=False)

    with pd.ExcelWriter("prediccion_pedidos.xlsx", engine="xlsxwriter") as writer:
        for df, hoja in zip(pedidos_predicciones, hojas_pedidos_pred):
            df["Fecha de Agotamiento"] = pd.to_datetime(df["Fecha de Agotamiento"]).dt.strftime('%d/%m/%Y')
            df["Fecha Pedido"] = pd.to_datetime(df["Fecha Pedido"]).dt.strftime('%d/%m/%Y')
            df["Fecha Recepción"] = pd.to_datetime(df["Fecha Recepción"]).dt.strftime('%d/%m/%Y')
            df.to_excel(writer, sheet_name=hoja, index=False)

    # Guardar el archivo de proveedores
    df_proveedores = pd.DataFrame(
        [{"Proveedor": p, "Lead Time (días)": t} for datos in proveedores.values() for p, t in datos]
    )
    with pd.ExcelWriter("proveedores.xlsx", engine="xlsxwriter") as writer:
        df_proveedores.to_excel(writer, sheet_name="proveedores", index=False)

# Proceso principal
historico, predicciones, pedidos_hist, pedidos_pred = [], [], [], []

for solvente in solventes:
    hist, pred, pedidos_h, pedidos_p = predecir_consumo_y_pedidos(solvente)
    historico.append(hist)
    predicciones.append(pred)
    pedidos_hist.append(pedidos_h)
    pedidos_pred.append(pedidos_p)

guardar_archivos_excel(historico, predicciones, pedidos_hist, pedidos_pred, proveedores)
print("Archivos generados exitosamente.")


// Función principal que se ejecuta periódicamente para traer los datos de Firebase
function getFirebaseData() {
    // URL de la API REST de Firebase
    var url = 'https://<your-project-id>/<your-firebase-project-name>.json'; // Reemplaza con tu URL
  
    // Realiza la solicitud GET a Firebase
    try {
      var response = UrlFetchApp.fetch(url);
      var data = JSON.parse(response.getContentText());
  
      // Comprobar si los datos son null y registrar un mensaje de advertencia
      if (data === null) {
        Logger.log("Advertencia: No se recibieron datos desde Firebase. Verifica la URL o las reglas de seguridad.");
      } else {
        // Loguea los datos recibidos de Firebase para verificar
        Logger.log(data);
      
        // Llama a la función para escribir datos en Google Sheets
        writeDataToSheet(data);
      }
    } catch (error) {
      Logger.log("Error al obtener datos de Firebase: " + error);
    }
  }
  
  // Función para escribir los datos en Google Sheets
  function writeDataToSheet(data) {
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
    sheet.clear(); // Limpiar los datos antiguos para actualizar con los nuevos
  
    // Configurar las celdas de título antes de los encabezados
    sheet.getRange('A1').setValue('CONTROL UNION').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle');
    sheet.getRange('A1:A2').merge(); // Combinar celdas A1 y A2 para el título "CONTROL UNION"
  
    sheet.getRange('B1:L1').merge().setValue('PLANILLA CONTROL DE REACTIVOS').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle');
    
    sheet.getRange('M1').setValue('Revisión').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle');
    sheet.getRange('N1').setValue('0').setHorizontalAlignment('center').setVerticalAlignment('middle');
  
    sheet.getRange('M2').setValue('Página').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle');
    sheet.getRange('N2').setValue('1/1').setHorizontalAlignment('center').setVerticalAlignment('middle');
  
    sheet.getRange('B2:L2').merge().setValue('Código PL-125').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle');
  
    // Insertar una "página vacía" entre el título y los encabezados
    sheet.insertRowsBefore(4, 3); // Añadir 3 filas vacías a partir de la fila 4
  
    // Combinar filas vacías 3, 4 y 5 desde A hasta N
    sheet.getRange('A3:N5').merge();
  
    // Agregar títulos combinados para las secciones
    sheet.getRange('A6:I6').merge().setValue('Datos del Reactivo').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle').setBackground('#4F4F4F').setFontColor('#FFFFFF'); // Color gris oscuro de fondo y texto blanco
  
    sheet.getRange('J6:N6').merge().setValue('Registros de Uso').setFontWeight('bold').setHorizontalAlignment('center').setVerticalAlignment('middle').setBackground('#4F4F4F').setFontColor('#FFFFFF'); // Color gris oscuro de fondo y texto blanco
  
    // Agregar bordes a las celdas A1:N2
    sheet.getRange('A1:N2').setBorder(true, true, true, true, true, true);
  
    // Agregar bordes a las celdas A6:N7
    sheet.getRange('A6:N7').setBorder(true, true, true, true, true, true);
  
    // Agregar encabezados de columna en la fila 7 (después de las filas vacías)
    var headers = ['Producto', 'Número', 'Alta', 'Marca', 'Código', 'Presentación', 'Lote', 'Vencimiento', 'Baja', 'Temperatura (°C)', 'Humedad (%)', 'Peso (g)', 'Fecha de Uso', 'Hora de Uso'];
    sheet.getRange(7, 1, 1, headers.length).setValues([headers]);
  
    // Aplicar negrita a los encabezados, centrado y ajuste de texto automáticamente
    var headerRange = sheet.getRange(7, 1, 1, headers.length);
    headerRange.setFontWeight('bold');
    headerRange.setHorizontalAlignment('center');
    headerRange.setVerticalAlignment('middle');
    headerRange.setWrap(true); // Ajustar texto automáticamente en los encabezados
  
    // Crear un array para almacenar los reactivos y luego ordenarlos por "Número"
    var reactivoArray = [];
    for (var key in data) {
      if (data.hasOwnProperty(key)) {
        var reactivo = data[key];
        reactivoArray.push(reactivo);
      }
    }
    
    // Ordenar el array por el "Número" del reactivo
    reactivoArray.sort(function(a, b) {
      return (a['02_numero'] || 0) - (b['02_numero'] || 0);
    });
  
    // Iterar sobre los datos ordenados y escribir en la hoja a partir de la fila 8
    var rowIndex = 8; // Comenzar desde la fila 8 para que haya una "página vacía"
    reactivoArray.forEach(function(reactivo) {
      var registrosUso = reactivo["Registros de Uso"];
      var numRegistros = registrosUso ? Object.keys(registrosUso).length : 1;
  
      // Insertar información principal del reactivo solo una vez
      var reactivoRowData = [
        reactivo['01_producto'] || '',
        reactivo['02_numero'] || '',
        reactivo['03_alta'] || '',
        reactivo['04_marca'] || '',
        reactivo['05_codigo'] || '',
        reactivo['06_presentacion'] || '',
        reactivo['07_lote'] || '',
        reactivo['08_vencimiento'] || '',
        reactivo['09_baja'] || '',
        '', // Temperatura se agregará después
        '', // Humedad se agregará después
        '', // Peso se agregará después
        '', // Fecha de uso se agregará después
        ''  // Hora de uso se agregará después
      ];
  
      // Escribir los datos principales del reactivo en la primera fila
      sheet.getRange(rowIndex, 1, 1, reactivoRowData.length).setValues([reactivoRowData]);
  
      // Ajustar el formato de las celdas
      var reactivoRange = sheet.getRange(rowIndex, 1, 1, reactivoRowData.length);
      reactivoRange.setHorizontalAlignment('center');
      reactivoRange.setVerticalAlignment('middle');
      reactivoRange.setWrap(true); // Ajustar texto automáticamente
  
      // Agregar bordes a la fila actual
      sheet.getRange(rowIndex, 1, 1, headers.length).setBorder(true, true, true, true, true, true);
  
      // Combinar las celdas si hay más de un registro de uso
      if (numRegistros > 1) {
        sheet.getRange(rowIndex, 1, numRegistros, 1).merge(); // Combinar Producto
        sheet.getRange(rowIndex, 2, numRegistros, 1).merge(); // Combinar Número
        sheet.getRange(rowIndex, 3, numRegistros, 1).merge(); // Combinar Alta
        sheet.getRange(rowIndex, 4, numRegistros, 1).merge(); // Combinar Marca
        sheet.getRange(rowIndex, 5, numRegistros, 1).merge(); // Combinar Código
        sheet.getRange(rowIndex, 6, numRegistros, 1).merge(); // Combinar Presentación
        sheet.getRange(rowIndex, 7, numRegistros, 1).merge(); // Combinar Lote
        sheet.getRange(rowIndex, 8, numRegistros, 1).merge(); // Combinar Vencimiento
        sheet.getRange(rowIndex, 9, numRegistros, 1).merge(); // Combinar Baja
      }
  
      // Escribir el primer registro de uso en la misma fila
      if (registrosUso) {
        var usoKeys = Object.keys(registrosUso);
  
        // Escribir el primer registro de uso en la misma fila
        if (usoKeys.length > 0) {
          var primerUso = registrosUso[usoKeys[0]];
          var pesoFormatted = primerUso['peso'] ? parseFloat(primerUso['peso']).toFixed(2) : '';
          sheet.getRange(rowIndex, 10, 1, 5).setValues([[primerUso['temperatura'] || '', primerUso['humedad'] || '', pesoFormatted, primerUso['fecha_uso'] || '', primerUso['hora_uso'] || '']]);
        }
  
        // Escribir los registros de uso adicionales en filas posteriores
        for (var i = 1; i < usoKeys.length; i++) {
          var uso = registrosUso[usoKeys[i]];
          var pesoFormatted = uso['peso'] ? parseFloat(uso['peso']).toFixed(2) : '';
  
          // Crear fila para el registro de uso
          var usoRowData = [
            '', // Producto no se repite en registros de uso
            '', // Número no se repite en registros de uso
            '', // Alta no se repite en registros de uso
            '', // Marca no se repite en registros de uso
            '', // Código no se repite en registros de uso
            '', // Presentación no se repite en registros de uso
            '', // Lote no se repite en registros de uso
            '', // Vencimiento no se repite en registros de uso
            '', // Baja no se repite en registros de uso
            uso['temperatura'] || '',  // Temperatura de este uso
            uso['humedad'] || '',      // Humedad de este uso
            pesoFormatted,             // Peso de este uso con dos decimales
            uso['fecha_uso'] || '',    // Fecha de uso
            uso['hora_uso'] || ''      // Hora de uso
          ];
  
          // Insertar la fila de uso
          sheet.getRange(rowIndex + 1, 1, 1, usoRowData.length).setValues([usoRowData]);
  
          // Ajustar el formato de las celdas de uso
          var usoRange = sheet.getRange(rowIndex + 1, 1, 1, usoRowData.length);
          usoRange.setHorizontalAlignment('center');
          usoRange.setVerticalAlignment('middle');
          usoRange.setWrap(true); // Ajustar texto automáticamente
  
          // Agregar bordes a la fila de uso
          sheet.getRange(rowIndex + 1, 1, 1, headers.length).setBorder(true, true, true, true, true, true);
  
          // Incrementar rowIndex para la próxima fila
          rowIndex++;
        }
      }
  
      // Incrementar rowIndex después de procesar los registros de uso
      rowIndex++;
    });
  
    // Aplicar ajuste de texto automáticamente a todas las celdas en la hoja
    var lastRow = sheet.getLastRow();
    var lastColumn = sheet.getLastColumn();
    sheet.getRange(1, 1, lastRow, lastColumn).setWrap(true); // Ajustar texto automáticamente en todas las celdas
  }
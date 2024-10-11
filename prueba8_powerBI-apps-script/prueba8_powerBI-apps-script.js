// Función principal que se ejecuta periódicamente para traer los datos de Firebase
function getFirebaseData() {
    // URL de la API REST de Firebase
    var url = 'https://<your-project-id>.firebaseio.com/reactivos.json'; // Reemplaza con tu URL
  
    // Realiza la solicitud GET a Firebase
    var response = UrlFetchApp.fetch(url);
    var data = JSON.parse(response.getContentText());
  
    // Llama a la función para escribir datos en Google Sheets
    writeDataToSheet(data);
  }
  
  // Función para escribir los datos en Google Sheets
  function writeDataToSheet(data) {
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
    sheet.clear(); // Limpiar los datos antiguos para actualizar con los nuevos
  
    // Agregar encabezados de columna
    var headers = ['Producto', 'Número', 'Alta', 'Marca', 'Código', 'Presentación', 'Lote', 'Vencimiento', 'Baja'];
    sheet.appendRow(headers);
  
    // Aplicar negrita a todos los encabezados
    var headerRange = sheet.getRange(1, 1, 1, headers.length);
    headerRange.setFontWeight('bold');
    headerRange.setHorizontalAlignment('center');
  
    // Iterar sobre los datos y escribir en la hoja
    var rowIndex = 2; // Comenzar desde la segunda fila, ya que la primera tiene encabezados
    for (var key in data) {
      if (data.hasOwnProperty(key)) {
        var reactivo = data[key];
        var rowData = [
          reactivo['01_producto'] || '',
          reactivo['02_numero'] || '',
          reactivo['03_alta'] || '',
          reactivo['04_marca'] || '',
          reactivo['05_codigo'] || '',
          reactivo['06_presentacion'] || '',
          reactivo['07_lote'] || '',
          reactivo['08_vencimiento'] || '',
          reactivo['09_baja'] || ''
        ];
  
        // Insertar la fila de datos
        sheet.appendRow(rowData);
  
        // Centrar la fila de datos recién agregada
        var dataRange = sheet.getRange(rowIndex, 1, 1, rowData.length);
        dataRange.setHorizontalAlignment('center');
  
        rowIndex++; // Moverse a la siguiente fila
      }
    }
  }
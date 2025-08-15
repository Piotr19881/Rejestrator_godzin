// ====================================================
// POPRAWIONY GOOGLE APPS SCRIPT - DODANO AUTORYZACJĘ
// ====================================================

function doGet(e) {
  try {
    var params = {};
    if (e && e.parameter) {
      params = e.parameter;
    }
    var action = params.action || 'test';

    // NOWA FUNKCJA: Autoryzacja przez parametr GET
    if (action === 'authorize') {
      return authorizeUser(params);
    }

    if (action === 'get_workers') {
      return getWorkers(params.workers_sheet_id);
    }
    if (action === 'get_alarms') {
      return getAlarms(params.alarms_sheet_id);
    }
    if (action === 'get_exceptions') {
      return getExceptions(params.exceptions_sheet_id);
    }

    return createResponse({
      status: 'success',
      message: 'Google Apps Script działa',
      action: action
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

function doPost(e) {
  try {
    var params = {};
    
    // Obsługa JSON POST
    if (e && e.postData && e.postData.contents) {
      try {
        var jsonData = JSON.parse(e.postData.contents);
        
        // ESP32-CAM wysyła {"authorization":"88080703299"}
        if (jsonData.authorization) {
          return authorizeByPESEL(jsonData.authorization);
        }
        
        params = jsonData;
      } catch (jsonError) {
        // Jeśli nie JSON, spróbuj parameter
        if (e.parameter) {
          params = e.parameter;
        }
      }
    } else if (e && e.parameter) {
      params = e.parameter;
    }

    var action = params.action || 'test';

    // NOWA FUNKCJA: Autoryzacja przez POST
    if (action === 'authorize' || params.authorization) {
      return authorizeUser(params);
    }

    if (action === 'save_hour_record') {
      return saveHourRecord(params);
    }
    if (action === 'save_exception') {
      return saveException(params);
    }

    return createResponse({
      status: 'success',
      message: 'POST request processed',
      action: action
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

// ====================================================
// NOWE FUNKCJE AUTORYZACJI
// ====================================================

function authorizeByPESEL(pesel) {
  try {
    console.log('Autoryzacja dla PESEL: ' + pesel);
    
    // ID arkusza pracowników
    var workersSheetId = '1G5Cna_eZMtIxADUxlXpJyd1GZAzDSLqRApaLOWEnMdo';
    var spreadsheet = SpreadsheetApp.openById(workersSheetId);
    var sheet = spreadsheet.getActiveSheet();
    var values = sheet.getDataRange().getValues();

    // Przeszukaj pracowników
    for (var i = 1; i < values.length; i++) {
      var row = values[i];
      var workerPesel = row[3] ? row[3].toString().trim() : '';
      var status = row[7] ? parseInt(row[7]) : 0;
      
      console.log('Sprawdzam PESEL: ' + workerPesel + ' vs ' + pesel);
      
      if (workerPesel === pesel.toString().trim() && status === 1) {
        var firstName = row[1] ? row[1].toString() : '';
        var lastName = row[2] ? row[2].toString() : '';
        
        console.log('Znaleziono: ' + firstName + ' ' + lastName);
        
        // Format odpowiedzi dla ESP32-CAM: {confirm;Imie;Nazwisko}
        var response = 'confirm;' + firstName + ';' + lastName;
        
        return ContentService
          .createTextOutput('{' + response + '}')
          .setMimeType(ContentService.MimeType.TEXT);
      }
    }

    console.log('Nie znaleziono pracownika dla PESEL: ' + pesel);
    
    // Brak autoryzacji: {denide}
    return ContentService
      .createTextOutput('{denide}')
      .setMimeType(ContentService.MimeType.TEXT);

  } catch (error) {
    console.error('Błąd autoryzacji: ' + error.toString());
    return ContentService
      .createTextOutput('{denide}')
      .setMimeType(ContentService.MimeType.TEXT);
  }
}

function authorizeUser(params) {
  var pesel = params.authorization || params.pesel || '';
  return authorizeByPESEL(pesel);
}

// ====================================================
// ISTNIEJĄCE FUNKCJE (bez zmian)
// ====================================================

function createResponse(data) {
  return ContentService
    .createTextOutput(JSON.stringify(data))
    .setMimeType(ContentService.MimeType.JSON);
}

function getWorkers(sheetId) {
  try {
    if (!sheetId) {
      sheetId = '1G5Cna_eZMtIxADUxlXpJyd1GZAzDSLqRApaLOWEnMdo';
    }
    var spreadsheet = SpreadsheetApp.openById(sheetId);
    var sheet = spreadsheet.getActiveSheet();
    var range = sheet.getDataRange();
    var values = range.getValues();

    var workers = [];
    for (var i = 1; i < values.length; i++) {
      var row = values[i];
      if (row[0] || row[1] || row[2]) {
        var worker = {
          id: row[0] ? row[0].toString() : '',
          firstName: row[1] ? row[1].toString() : '',
          lastName: row[2] ? row[2].toString() : '',
          pesel: row[3] ? row[3].toString() : '',
          rfid1: row[4] ? row[4].toString() : '',
          rfid2: row[5] ? row[5].toString() : '',
          rfid3: row[6] ? row[6].toString() : '',
          status: row[7] ? parseInt(row[7]) : 1
        };
        workers.push(worker);
      }
    }

    return createResponse({
      status: 'success',
      workers: workers,
      count: workers.length,
      sheetId: sheetId
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString(),
      sheetId: sheetId
    });
  }
}

function getAlarms(sheetId) {
  try {
    if (!sheetId) {
      sheetId = '1ZsgBpAhGqQ4yRA6kiMEb64D4xZ_ZxYQxFFxLGYHo0wk';
    }
    var spreadsheet = SpreadsheetApp.openById(sheetId);
    var sheet = spreadsheet.getActiveSheet();
    var values = sheet.getDataRange().getValues();

    var alarms = [];
    for (var i = 1; i < values.length; i++) {
      var row = values[i];
      if (row[0] || row[1]) {
        alarms.push({
          lp: row[0] ? row[0].toString() : '',
          hour: row[1] ? row[1].toString() : '',
          type: row[2] ? parseInt(row[2]) : 0,
          status: row[3] ? parseInt(row[3]) : 1
        });
      }
    }

    return createResponse({
      status: 'success',
      alarms: alarms,
      count: alarms.length
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

function getExceptions(sheetId) {
  try {
    if (!sheetId) {
      sheetId = '1lXX94aGhszQ9vLMOVs4HQp2D_kTAENaahUIFy89cEUM';
    }
    var spreadsheet = SpreadsheetApp.openById(sheetId);
    var sheet = spreadsheet.getActiveSheet();
    var values = sheet.getDataRange().getValues();

    var exceptions = [];
    for (var i = 1; i < values.length; i++) {
      var row = values[i];
      if (row[0] || row[1]) {
        exceptions.push({
          id: row[0] ? row[0].toString() : '',
          type: row[1] ? row[1].toString() : '',
          description: row[2] ? row[2].toString() : '',
          active: row[3] ? parseInt(row[3]) : 1
        });
      }
    }

    return createResponse({
      status: 'success',
      exceptions: exceptions,
      count: exceptions.length
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

function saveHourRecord(params) {
  try {
    var hoursSheetId = params.hours_sheet_id || '1fJCabvonk7omA-AAxdNvJ8RNurIV-N02QQL95GwC7ms';
    var spreadsheet = SpreadsheetApp.openById(hoursSheetId);
    var sheet = spreadsheet.getActiveSheet();

    var rowData = [
      params.lp || '',
      params.firstName || '',
      params.lastName || '',
      params.checkinTime || new Date().toLocaleString(),
      params.presenceStatus || '',
      params.authCode || '',
      params.workTime || ''
    ];

    sheet.appendRow(rowData);

    return createResponse({
      status: 'success',
      message: 'Hour record saved successfully'
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

function saveException(params) {
  try {
    var exceptionsSheetId = params.exceptions_sheet_id || '1lXX94aGhszQ9vLMOVs4HQp2D_kTAENaahUIFy89cEUM';
    var spreadsheet = SpreadsheetApp.openById(exceptionsSheetId);
    var sheet = spreadsheet.getActiveSheet();

    var rowData = [
      params.id || '',
      params.type || '',
      params.description || '',
      params.active || 1
    ];

    sheet.appendRow(rowData);

    return createResponse({
      status: 'success',
      message: 'Exception saved successfully'
    });

  } catch (error) {
    return createResponse({
      status: 'error',
      message: error.toString()
    });
  }
}

// ====================================================
// FUNKCJE TESTOWE
// ====================================================

function testAuthorization() {
  // Test autoryzacji Piotra Prokopa
  var result = authorizeByPESEL('88080703299');
  console.log('Test result: ' + result.getContent());
  
  // Test nieistniejącego PESEL
  var result2 = authorizeByPESEL('12345678901');
  console.log('Test result 2: ' + result2.getContent());
}

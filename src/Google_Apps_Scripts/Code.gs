/*
* Filename: Código.gs
*/
function doPost(e) {
  try {
    //e = {date:"2024-11-19T17:58:52Z",sync_time:1732039129,diff_t:0,index:0,display:1,f_heap:267816,id:"M5Dial"};
    // Note: display (state): 1 = Awake; 0 = Asleep
    var my_debug = false;
    if (my_debug)
      Logger = BetterLog.useSpreadsheet('<ID_of_a_logging_spreadsheet>'); // Sheet "Logger" (https://github.com/peterherrmann/BetterLog)
    // 2024-11-12: BetterLog installed here in Apps Script - Bibliotecas + ID '1DSyxam1ceq72bMHsE6aOVeOl94X78WCwiYPytKi7chlg4x5GqiNXSw0l'
    if (my_debug)
      Logger.log("e = %s", e);

    var params = e.postData.contents;
    if (my_debug)
      Logger.log("params= %s\n", params);

    var result = 'Ok'; // assume success

    if (params == undefined) {
      result = 'No Parameters';
    } else {
      // Spreadsheet location my_drive/M5Dial_Timezones_SNTP_sync_times
      var id = '<Spreadsheet_ID>'; // Spreadsheet ID
      var sheet = SpreadsheetApp.openById(id).getActiveSheet();
      var newRow = sheet.getLastRow() + 1;
      var rowData = [];
      
      // Parse the main JSON string 
      var jsonObj = JSON.parse(params);
      if (my_debug)
        Logger.log("jsonObj= %s\n", jsonObj);

      var dataObj = jsonObj.data;
      if (my_debug)
        Logger.log("dataObj= %s\n", dataObj);

      // Extract individual elements from data
      rowData[0] = dataObj.date;

      rowData[1] = dataObj.sync_time;

      rowData[2] = dataObj.diff_t;

      rowData[3] = dataObj.index;

      rowData[4] = dataObj.display;

      rowData[5] = dataObj.f_heap;

      rowData[6] = dataObj.id;

      rowData.push(new Date()); // Add the datetime of execution of this macro
      if (my_debug)
        Logger.log("rowData = %s\n", JSON.stringify(rowData));
      // Write new row below
      var newRange = sheet.getRange(newRow, 1, 1, rowData.length);
      newRange.setValues([rowData]);
    }

    if (my_debug)
      Logger.log("result = %s\n", result);

    // Create result object
    var result = { 
      status: "success", 
      data: dataObj 
    };

    // Return result of operation
    return ContentService.createTextOutput(result);

  } catch (error) {
    if (my_debug)
      Logger.log("Error: " + error.message);
    
    var errorResult = { 
      status: "error", 
      message: error.message
    };
    
    return ContentService.createTextOutput(result);
  } 
}

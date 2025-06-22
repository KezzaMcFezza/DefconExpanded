
//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 01-04-2025

function createDialogElements() {
    const dialogHTML = `
      <div id="custom-dialog" class="custom-dialog">
        <div class="dialog-content">
          <label id="dialog-message"></label>
          <div class="dialog-buttons">
            <button id="dialog-cancel" class="dialog-button">Cancel</button>
            <button id="dialog-confirm" class="dialog-button">OK</button>
          </div>
        </div>
      </div>
    `;
  
    const styles = `
      <style>
        .custom-dialog {
          display: none;
          position: fixed;
          z-index: 1000;
          left: 0;
          top: 0;
          width: 100%;
          height: 100%;
          background-color: rgba(0,0,0,0.4);
        }
        .dialog-content {
          background-color: #101010;
          margin: 15% auto;
          padding: 20px;
          width: 350px;
          border-radius: 10px;
          text-align: center;
        }
        .dialog-message {
          color: #1a1a1a;
        }
        #dialog-cancel {
          background-color: #830000;
          color: #ffffff;
        }
        #dialog-confirm {
          background-color: #0067b8;
          color: #ffffff;
        }
        .dialog-button {
          font-size: 0.9rem;
          font-weight: bold;
          padding: 5px 20px;
          margin: 20px 10px 0px 10px;
          border-radius: 10px;
          cursor: pointer;
          border: none;
        }
      </style>
    `;
  
    
    if (!document.getElementById('custom-dialog')) {
      document.body.insertAdjacentHTML('beforeend', dialogHTML);
      document.head.insertAdjacentHTML('beforeend', styles);
    }
  }
  
  function customConfirm(message) {
    return new Promise((resolve) => {
      const dialog = document.getElementById('custom-dialog');
      const messageElement = document.getElementById('dialog-message');
      const confirmButton = document.getElementById('dialog-confirm');
      const cancelButton = document.getElementById('dialog-cancel');
  
      messageElement.textContent = message;
      dialog.style.display = 'block';
      cancelButton.style.display = 'inline-block';
  
      const closeDialog = (result) => {
        dialog.style.display = 'none';
        resolve(result);
      };
  
      confirmButton.onclick = () => closeDialog(true);
      cancelButton.onclick = () => closeDialog(false);
    });
  }
  
  function customAlert(message) {
    return new Promise((resolve) => {
      const dialog = document.getElementById('custom-dialog');
      const messageElement = document.getElementById('dialog-message');
      const confirmButton = document.getElementById('dialog-confirm');
      const cancelButton = document.getElementById('dialog-cancel');
  
      messageElement.textContent = message;
      dialog.style.display = 'block';
      cancelButton.style.display = 'none';
  
      const closeDialog = () => {
        dialog.style.display = 'none';
        resolve();
      };
  
      confirmButton.onclick = closeDialog;
    });
  }
  
  function initializePopupSystem() {
    createDialogElements();
    
    
    window.confirm = customConfirm;
    window.alert = customAlert;
  }
  
  export {
    customConfirm,
    customAlert,
    initializePopupSystem
  };
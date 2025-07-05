// WSPR Encoder Test Page Logic

document.addEventListener('DOMContentLoaded', () => {
  const encodeBtn = document.getElementById('encode-btn');
  const encodeStatus = document.getElementById('encode-status');
  const resultsSection = document.getElementById('results-section');
  const symbolsDisplay = document.getElementById('symbols-display');
  const copySymbolsBtn = document.getElementById('copy-symbols-btn');
  const downloadSymbolsBtn = document.getElementById('download-symbols-btn');
  
  let lastEncodingResult = null;
  
  // Load settings to populate form fields
  async function loadCurrentSettings() {
    try {
      const response = await fetch('/api/settings');
      if (response.ok) {
        const settings = await response.json();
        
        if (settings.call) document.getElementById('test-callsign').value = settings.call;
        if (settings.loc) document.getElementById('test-locator').value = settings.loc;
        if (settings.pwr) document.getElementById('test-power').value = settings.pwr;
        
        // Use current band frequency if available
        if (settings.freq) {
          document.getElementById('test-frequency').value = settings.freq;
        }
      }
    } catch (error) {
      console.warn('Failed to load current settings:', error);
    }
  }
  
  // Encode WSPR message
  async function encodeWsprMessage() {
    const callsign = document.getElementById('test-callsign').value.trim();
    const locator = document.getElementById('test-locator').value.trim();
    const powerDbm = parseInt(document.getElementById('test-power').value);
    const frequency = parseInt(document.getElementById('test-frequency').value);
    
    // Validate inputs
    if (!callsign || !locator) {
      encodeStatus.textContent = 'Error: Callsign and locator are required';
      encodeStatus.style.color = '#f44336';
      return;
    }
    
    if (isNaN(powerDbm) || powerDbm < 0 || powerDbm > 60) {
      encodeStatus.textContent = 'Error: Power must be between 0 and 60 dBm';
      encodeStatus.style.color = '#f44336';
      return;
    }
    
    if (isNaN(frequency) || frequency < 1000000 || frequency > 30000000) {
      encodeStatus.textContent = 'Error: Frequency must be between 1 MHz and 30 MHz';
      encodeStatus.style.color = '#f44336';
      return;
    }
    
    // Show encoding in progress
    encodeBtn.disabled = true;
    encodeBtn.textContent = 'Encoding...';
    encodeStatus.textContent = 'Encoding WSPR message...';
    encodeStatus.style.color = '#2196f3';
    
    try {
      const requestData = {
        callsign: callsign,
        locator: locator,
        powerDbm: powerDbm,
        frequency: frequency
      };
      
      const response = await fetch('/api/wspr/encode', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(requestData)
      });
      
      const result = await response.json();
      
      if (response.ok && result.success) {
        lastEncodingResult = result;
        displayEncodingResults(result);
        encodeStatus.textContent = `Encoded successfully: ${result.symbolCount} symbols`;
        encodeStatus.style.color = '#4caf50';
      } else {
        encodeStatus.textContent = `Encoding failed: ${result.error || 'Unknown error'}`;
        encodeStatus.style.color = '#f44336';
        resultsSection.style.display = 'none';
      }
      
    } catch (error) {
      console.error('WSPR encoding error:', error);
      encodeStatus.textContent = `Network error: ${error.message}`;
      encodeStatus.style.color = '#f44336';
      resultsSection.style.display = 'none';
    } finally {
      encodeBtn.disabled = false;
      encodeBtn.textContent = 'Encode WSPR Message';
    }
  }
  
  // Display encoding results
  function displayEncodingResults(result) {
    document.getElementById('result-symbol-count').textContent = result.symbolCount;
    document.getElementById('result-tone-spacing').textContent = result.toneSpacing;
    document.getElementById('result-symbol-period').textContent = result.symbolPeriod;
    document.getElementById('result-duration').textContent = result.transmissionDurationSeconds.toFixed(1);
    
    // Display symbols in a grid format
    symbolsDisplay.innerHTML = '';
    
    // Add symbols in groups of 10 for readability
    const symbols = result.symbols;
    for (let i = 0; i < symbols.length; i += 10) {
      const row = document.createElement('div');
      row.className = 'symbol-row';
      
      const rowLabel = document.createElement('span');
      rowLabel.className = 'symbol-row-label';
      rowLabel.textContent = `${i.toString().padStart(3, '0')}-${Math.min(i + 9, symbols.length - 1).toString().padStart(3, '0')}:`;
      row.appendChild(rowLabel);
      
      for (let j = i; j < Math.min(i + 10, symbols.length); j++) {
        const symbolSpan = document.createElement('span');
        symbolSpan.className = `symbol symbol-${symbols[j]}`;
        symbolSpan.textContent = symbols[j];
        symbolSpan.title = `Symbol ${j}: ${symbols[j]} (${symbols[j] * result.toneSpacing} Hz offset)`;
        row.appendChild(symbolSpan);
      }
      
      symbolsDisplay.appendChild(row);
    }
    
    resultsSection.style.display = 'block';
  }
  
  // Copy symbols to clipboard
  async function copySymbolsToClipboard() {
    if (!lastEncodingResult) {
      alert('No encoding result to copy');
      return;
    }
    
    const symbolsText = lastEncodingResult.symbols.join(' ');
    
    try {
      await navigator.clipboard.writeText(symbolsText);
      copySymbolsBtn.textContent = 'Copied!';
      setTimeout(() => {
        copySymbolsBtn.textContent = 'Copy Symbols';
      }, 2000);
    } catch (error) {
      console.error('Failed to copy to clipboard:', error);
      // Fallback method
      const textArea = document.createElement('textarea');
      textArea.value = symbolsText;
      document.body.appendChild(textArea);
      textArea.select();
      document.execCommand('copy');
      document.body.removeChild(textArea);
      
      copySymbolsBtn.textContent = 'Copied!';
      setTimeout(() => {
        copySymbolsBtn.textContent = 'Copy Symbols';
      }, 2000);
    }
  }
  
  // Download encoding result as JSON
  function downloadEncodingResult() {
    if (!lastEncodingResult) {
      alert('No encoding result to download');
      return;
    }
    
    const dataStr = JSON.stringify(lastEncodingResult, null, 2);
    const dataBlob = new Blob([dataStr], {type: 'application/json'});
    
    const link = document.createElement('a');
    link.href = URL.createObjectURL(dataBlob);
    link.download = `wspr-encoding-${lastEncodingResult.callsign}-${Date.now()}.json`;
    link.click();
    
    downloadSymbolsBtn.textContent = 'Downloaded!';
    setTimeout(() => {
      downloadSymbolsBtn.textContent = 'Download as JSON';
    }, 2000);
  }
  
  // Event listeners
  encodeBtn.addEventListener('click', encodeWsprMessage);
  copySymbolsBtn.addEventListener('click', copySymbolsToClipboard);
  downloadSymbolsBtn.addEventListener('click', downloadEncodingResult);
  
  // Allow Enter key to trigger encoding
  document.addEventListener('keypress', (event) => {
    if (event.key === 'Enter' && !encodeBtn.disabled) {
      encodeWsprMessage();
    }
  });
  
  // Load current settings on page load
  loadCurrentSettings();
});
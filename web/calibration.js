// Si5351 Calibration Page JavaScript

class CalibrationManager {
    constructor() {
        this.testActive = false;
        this.baseFrequency = 10000000; // Default 10 MHz
        this.currentFrequency = 10000000;
        this.correctionPPM = 0;
        
        this.elements = {
            testFrequency: document.getElementById('test-frequency'),
            startTest: document.getElementById('start-test'),
            stopTest: document.getElementById('stop-test'),
            currentFrequency: document.getElementById('current-frequency'),
            correctionValue: document.getElementById('correction-value'),
            applyCorrection: document.getElementById('apply-correction'),
            saveCalibration: document.getElementById('save-calibration'),
            resetCalibration: document.getElementById('reset-calibration'),
            calibrationMessage: document.getElementById('calibration-message')
        };
        
        this.initializeEventListeners();
        this.loadCurrentSettings();
    }
    
    initializeEventListeners() {
        // Test frequency controls
        this.elements.startTest.addEventListener('click', () => this.startTestSignal());
        this.elements.stopTest.addEventListener('click', () => this.stopTestSignal());
        
        // Frequency presets
        document.querySelectorAll('.freq-preset').forEach(button => {
            button.addEventListener('click', (e) => {
                const frequency = parseInt(e.target.dataset.freq);
                this.setTestFrequency(frequency);
            });
        });
        
        // Fine tuning buttons
        document.querySelectorAll('.adj-btn').forEach(button => {
            button.addEventListener('click', (e) => {
                const step = parseInt(e.target.dataset.step);
                this.adjustFrequency(step);
            });
        });
        
        // Calibration controls
        this.elements.applyCorrection.addEventListener('click', () => this.applyCorrection());
        this.elements.saveCalibration.addEventListener('click', () => this.saveCalibration());
        this.elements.resetCalibration.addEventListener('click', () => this.resetCalibration());
        
        // Input field changes
        this.elements.testFrequency.addEventListener('change', () => {
            this.baseFrequency = parseInt(this.elements.testFrequency.value);
            this.currentFrequency = this.baseFrequency;
            this.updateDisplay();
        });
        
        this.elements.correctionValue.addEventListener('change', () => {
            this.correctionPPM = parseFloat(this.elements.correctionValue.value);
        });
    }
    
    async loadCurrentSettings() {
        try {
            const response = await fetch('/api/settings');
            const settings = await response.json();
            
            if (settings.si5351Correction !== undefined) {
                this.correctionPPM = settings.si5351Correction;
                this.elements.correctionValue.value = this.correctionPPM;
                this.showMessage(`Current calibration: ${this.correctionPPM} PPM`, 'info');
            }
        } catch (error) {
            console.error('Failed to load settings:', error);
            this.showMessage('Failed to load current calibration settings', 'error');
        }
    }
    
    setTestFrequency(frequency) {
        this.baseFrequency = frequency;
        this.currentFrequency = frequency;
        this.elements.testFrequency.value = frequency;
        this.updateDisplay();
        
        if (this.testActive) {
            this.sendFrequencyUpdate();
        }
    }
    
    adjustFrequency(step) {
        if (!this.testActive) {
            this.showMessage('Start test signal first', 'warning');
            return;
        }
        
        this.currentFrequency += step;
        this.updateDisplay();
        this.sendFrequencyUpdate();
        
        // Calculate the correction in PPM
        const deviation = this.currentFrequency - this.baseFrequency;
        const ppm = (deviation / this.baseFrequency) * 1000000;
        this.correctionPPM = Math.round(ppm * 10) / 10; // Round to 1 decimal place
        this.elements.correctionValue.value = this.correctionPPM;
    }
    
    updateDisplay() {
        this.elements.currentFrequency.textContent = this.currentFrequency.toLocaleString() + ' Hz';
    }
    
    async startTestSignal() {
        try {
            const response = await fetch('/api/calibration/start', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    frequency: this.currentFrequency
                })
            });
            
            if (response.ok) {
                this.testActive = true;
                this.elements.startTest.disabled = true;
                this.elements.stopTest.disabled = false;
                this.showMessage(`Test signal started at ${(this.currentFrequency / 1000000).toFixed(6)} MHz`, 'success');
            } else {
                throw new Error('Failed to start test signal');
            }
        } catch (error) {
            console.error('Error starting test signal:', error);
            this.showMessage('Failed to start test signal', 'error');
        }
    }
    
    async stopTestSignal() {
        try {
            const response = await fetch('/api/calibration/stop', {
                method: 'POST'
            });
            
            if (response.ok) {
                this.testActive = false;
                this.elements.startTest.disabled = false;
                this.elements.stopTest.disabled = true;
                this.showMessage('Test signal stopped', 'info');
            } else {
                throw new Error('Failed to stop test signal');
            }
        } catch (error) {
            console.error('Error stopping test signal:', error);
            this.showMessage('Failed to stop test signal', 'error');
        }
    }
    
    async sendFrequencyUpdate() {
        if (!this.testActive) return;
        
        try {
            await fetch('/api/calibration/adjust', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    frequency: this.currentFrequency
                })
            });
        } catch (error) {
            console.error('Error updating frequency:', error);
        }
    }
    
    async applyCorrection() {
        try {
            const response = await fetch('/api/calibration/correction', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    correction: this.correctionPPM
                })
            });
            
            if (response.ok) {
                this.showMessage(`Applied correction: ${this.correctionPPM} PPM`, 'success');
                
                // If test is active, restart with corrected frequency
                if (this.testActive) {
                    this.stopTestSignal().then(() => {
                        setTimeout(() => this.startTestSignal(), 500);
                    });
                }
            } else {
                throw new Error('Failed to apply correction');
            }
        } catch (error) {
            console.error('Error applying correction:', error);
            this.showMessage('Failed to apply correction', 'error');
        }
    }
    
    async saveCalibration() {
        try {
            // First apply the correction
            await this.applyCorrection();
            
            // Then save to settings
            const settings = await fetch('/api/settings').then(r => r.json());
            settings.si5351Correction = this.correctionPPM;
            
            const response = await fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(settings)
            });
            
            if (response.ok) {
                this.showMessage(`Calibration saved: ${this.correctionPPM} PPM (will be used for WSPR transmissions)`, 'success');
            } else {
                throw new Error('Failed to save calibration');
            }
        } catch (error) {
            console.error('Error saving calibration:', error);
            this.showMessage('Failed to save calibration to settings', 'error');
        }
    }
    
    async resetCalibration() {
        this.correctionPPM = 0;
        this.elements.correctionValue.value = 0;
        this.currentFrequency = this.baseFrequency;
        this.updateDisplay();
        
        await this.applyCorrection();
        this.showMessage('Calibration reset to 0 PPM', 'info');
    }
    
    showMessage(message, type = 'info') {
        const messageElement = this.elements.calibrationMessage;
        messageElement.textContent = message;
        messageElement.className = `message ${type}`;
        
        // Clear message after 5 seconds
        setTimeout(() => {
            messageElement.textContent = '';
            messageElement.className = '';
        }, 5000);
    }
}

// Initialize calibration manager when page loads
document.addEventListener('DOMContentLoaded', () => {
    new CalibrationManager();
});
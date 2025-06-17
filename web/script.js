document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('settings-form');
    const statusMessage = document.getElementById('status-message');
    const submitBtn = document.getElementById('submit-btn');

    form.addEventListener('submit', async (event) => {
        event.preventDefault();
        submitBtn.disabled = true;
        submitBtn.textContent = 'Saving...';

        const formData = new FormData(form);
        const data = new URLSearchParams(formData);

        try {
            const response = await fetch('/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: data,
            });

            if (response.ok) {
                statusMessage.textContent = 'Settings saved successfully! The device will now reboot.';
                statusMessage.style.color = 'green';
                // The device will handle rebooting.
            } else {
                const errorText = await response.text();
                throw new Error(errorText || 'Failed to save settings.');
            }
        } catch (error) {
            statusMessage.textContent = `Error: ${error.message}`;
            statusMessage.style.color = 'red';
            submitBtn.disabled = false;
            submitBtn.textContent = 'Save and Reboot';
        }
    });
});

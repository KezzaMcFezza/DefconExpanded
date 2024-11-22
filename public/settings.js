document.addEventListener('DOMContentLoaded', async () => {
    try {
        const currentUserResponse = await fetch('/api/current-user');
        const currentUserData = await currentUserResponse.json();

        if (currentUserData && currentUserData.user && currentUserData.user.username) {
            document.title = `${currentUserData.user.username}'s Settings - DEFCON Expanded`;
        } else {
            document.title = 'Settings - DEFCON Expanded'; 
        }
    } catch (error) {
        console.error('Error fetching current user:', error);
        document.title = 'Settings - DEFCON Expanded'; 
    }

    const changePasswordForm = document.getElementById('change-password-form');
    const changeEmailForm = document.getElementById('change-email-form');
    const changeUsernameForm = document.getElementById('change-username-form');
    const leaderboardNameChangeForm = document.getElementById('leaderboard-name-change-form');
    const requestBlacklistBtn = document.getElementById('request-blacklist');
    const requestDeleteAccountBtn = document.getElementById('request-delete-account');
    handlePasswordChangeToken();

    document.getElementById('request-password-change-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const email = document.getElementById('email').value;
    
        try {
            const response = await fetch('/api/request-password-change', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ email })
            });
    
            const data = await response.json();
    
            if (response.ok) {
                alert(data.message);
            } else {
                alert(data.error);
            }
        } catch (error) {
            console.error('Error:', error);
            alert('An error occurred while requesting password change');
        }
    });
    
    function handlePasswordChangeToken() {
        const urlParams = new URLSearchParams(window.location.search);
        const token = urlParams.get('token');
        if (token) {
            document.getElementById('request-password-change-form').style.display = 'none';
            document.getElementById('change-password-form').style.display = 'flex';
        }
    }
    
    document.getElementById('change-password-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const newPassword = document.getElementById('new-password').value;
        const confirmPassword = document.getElementById('confirm-password').value;
    
        if (newPassword !== confirmPassword) {
            alert('New passwords do not match');
            return;
        }
    
        const urlParams = new URLSearchParams(window.location.search);
        const token = urlParams.get('token');
    
        try {
            const response = await fetch(`/api/change-password?token=${token}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ newPassword, confirmPassword }) 
            });
    
            const data = await response.json();
    
            if (response.ok) {
                alert(data.message);
            } else {
                alert(data.error);
            }
        } catch (error) {
            console.error('Error:', error);
            alert('An error occurred while changing the password');
        }
    });

    changeEmailForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const newEmail = document.getElementById('new-email').value;

        try {
            const response = await fetch('/api/request-email-change', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ newEmail })
            });
            const data = await response.json();
            if (response.ok) {
                alert(data.message);
            } else {
                alert(data.error);
            }
        } catch (error) {
            console.error('Error:', error);
            alert('An error occurred while submitting the email change request');
        }
    });

    changeUsernameForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const newUsername = document.getElementById('new-username').value;

        try {
            const response = await fetch('/api/request-username-change', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ newUsername })
            });
            const data = await response.json();
            if (response.ok) {
                alert(data.message);
            } else {
                alert(data.error);
            }
        } catch (error) {
            console.error('Error:', error);
            alert('An error occurred while submitting the username change request');
        }
    });

    leaderboardNameChangeForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const newName = document.getElementById('new-leaderboard-name').value;
        if (newName) {
            try {
                const response = await fetch('/api/request-leaderboard-name-change', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ newName })
                });
                const data = await response.json();
                alert(data.message);
                document.getElementById('new-leaderboard-name').value = '';
            } catch (error) {
                console.error('Error:', error);
                alert('An error occurred while submitting the request');
            }
        }
    });

    requestBlacklistBtn.addEventListener('click', async () => {
        const confirmed = await new Promise(resolve => {
            resolve(confirm('Are you sure you want to request to be blacklisted from the leaderboard?'));
        });
    
        if (confirmed) {
            try {
                const response = await fetch('/api/request-blacklist', {
                    method: 'POST'
                });
                const data = await response.json();
                alert(data.message);
            } catch (error) {
                console.error('Error:', error);
                alert('An error occurred while submitting the request');
            }
        }
    });

    requestDeleteAccountBtn.addEventListener('click', async () => {
        const confirmed = await new Promise(resolve => {
            resolve(confirm('Are you sure you want to request account deletion? This action cannot be undone.'));
        });
    
        if (confirmed) {
            try {
                const response = await fetch('/api/request-account-deletion', {
                    method: 'POST'
                });
                const data = await response.json();
                alert(data.message);
            } catch (error) {
                console.error('Error:', error);
                alert('An error occurred while submitting the request');
            }
        }
    });
});

function getFriendlyRequestType(type) {
    switch (type) {
        case 'leaderboard_name_change':
            return 'You have requested a change to your leaderboard name';
        case 'blacklist':
            return 'You have requested to be added to the blacklist';
        case 'account_deletion':
            return 'You have requested your account to be deleted';
        case 'username_change':
            return 'You have requested to change your username';
        case 'email_change':
            return 'You have requested to change your email address';
        default:
            return 'Unknown request type';
    }
}

function getRequestDetails(request) {
    switch (request.type) {
        case 'leaderboard_name_change':
            return `New leaderboard name: ${request.requested_name}`;
        case 'username_change':
            return `New username: ${request.requested_username}`;
        case 'email_change':
            return `New email: ${request.requested_email}`;
        default:
            return ''; 
    }
}

async function loadUserPendingRequests() {
    try {
        const response = await fetch('/api/user-pending-requests');
        const requests = await response.json();

        const requestContainer = document.querySelector('#user-pending-requests-container');
        requestContainer.innerHTML = '';

        if (requests.length === 0) {
            requestContainer.innerHTML = '<p>No requests found.</p>';
        } else {
            requests.forEach(request => {
                const requestElement = document.createElement('div');
                requestElement.classList.add('pending-request');

                const statusClass = request.status === 'pending' ? 'pending' : request.status === 'approved' ? 'approved' : 'rejected';
                const friendlyType = getFriendlyRequestType(request.type);
                const requestDetails = getRequestDetails(request); 

                requestElement.innerHTML = `
                    <div class="request-card">
                        <p class="request-type">${friendlyType}</p>
                        ${requestDetails ? `<p class="request-details">${requestDetails}</p>` : ''}
                        <p class="request-status ${statusClass}">Status: ${request.status}</p>
                    </div>
                `;

                requestContainer.appendChild(requestElement);
            });
        }
    } catch (error) {
        console.error('Error loading user pending requests:', error);
    }
}

document.addEventListener('DOMContentLoaded', loadUserPendingRequests);






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

document.addEventListener('DOMContentLoaded', async () => {
    initializeUI();
    setupFormListeners();
    handlePasswordChangeToken();
    loadUserPendingRequests();
});

async function initializeUI() {
    try {
        
        const currentUserResponse = await fetch('/api/current-user');
        const currentUserData = await currentUserResponse.json();

        if (currentUserData?.user?.username) {
            document.title = `${currentUserData.user.username}'s Settings - DEFCON Expanded`;
        } else {
            document.title = 'Settings - DEFCON Expanded';
        }
    } catch (error) {
        console.error('Error fetching current user:', error);
        document.title = 'Settings - DEFCON Expanded';
    }
}

function setupFormListeners() {
    
    const requestPasswordChangeForm = document.getElementById('request-password-change-form');
    if (requestPasswordChangeForm) {
        requestPasswordChangeForm.addEventListener('submit', handleRequestPasswordChange);
    }

    
    const changePasswordForm = document.getElementById('change-password-form');
    if (changePasswordForm) {
        changePasswordForm.addEventListener('submit', handlePasswordChange);
    }

    
    const changeEmailForm = document.getElementById('change-email-form');
    if (changeEmailForm) {
        changeEmailForm.addEventListener('submit', handleEmailChange);
    }

    
    const changeUsernameForm = document.getElementById('change-username-form');
    if (changeUsernameForm) {
        changeUsernameForm.addEventListener('submit', handleUsernameChange);
    }

    
    const requestBlacklistBtn = document.getElementById('request-blacklist');
    if (requestBlacklistBtn) {
        requestBlacklistBtn.addEventListener('click', handleBlacklistRequest);
    }

    
    const requestDeleteAccountBtn = document.getElementById('request-delete-account');
    if (requestDeleteAccountBtn) {
        requestDeleteAccountBtn.addEventListener('click', handleAccountDeletionRequest);
    }
}

function handlePasswordChangeToken() {
    const urlParams = new URLSearchParams(window.location.search);
    const token = urlParams.get('token');

    if (token) {
        const requestPasswordChangeForm = document.getElementById('request-password-change-form');
        const changePasswordForm = document.getElementById('change-password-form');

        if (requestPasswordChangeForm && changePasswordForm) {
            requestPasswordChangeForm.style.display = 'none';
            changePasswordForm.style.display = 'flex';
        }
    }
}

async function handleRequestPasswordChange(e) {
    e.preventDefault();
    const emailInput = document.getElementById('email');
    const submitButton = e.target.querySelector('button[type="submit"]');

    if (!emailInput || !submitButton) return;

    const email = emailInput.value;

    
    submitButton.disabled = true;
    submitButton.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Sending...';

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
            showNotification('success', data.message);
            emailInput.value = '';
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while requesting password change');
    } finally {
        
        submitButton.disabled = false;
        submitButton.innerHTML = '<i class="fas fa-paper-plane"></i> Request Password Change';
    }
}

async function handlePasswordChange(e) {
    e.preventDefault();
    const newPasswordInput = document.getElementById('new-password');
    const confirmPasswordInput = document.getElementById('confirm-password');
    const submitButton = e.target.querySelector('button[type="submit"]');

    if (!newPasswordInput || !confirmPasswordInput || !submitButton) return;

    const newPassword = newPasswordInput.value;
    const confirmPassword = confirmPasswordInput.value;

    if (newPassword !== confirmPassword) {
        showNotification('error', 'New passwords do not match');
        highlightField(confirmPasswordInput);
        return;
    }

    const urlParams = new URLSearchParams(window.location.search);
    const token = urlParams.get('token');

    if (!token) {
        showNotification('error', 'Password change token is missing');
        return;
    }

    
    submitButton.disabled = true;
    submitButton.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Changing...';

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
            showNotification('success', data.message);
            newPasswordInput.value = '';
            confirmPasswordInput.value = '';

            
            setTimeout(() => {
                window.location.href = '/login';
            }, 3000);
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while changing the password');
    } finally {
        
        submitButton.disabled = false;
        submitButton.innerHTML = '<i class="fas fa-key"></i> Change Password';
    }
}

async function handleEmailChange(e) {
    e.preventDefault();
    const newEmailInput = document.getElementById('new-email');
    const submitButton = e.target.querySelector('button[type="submit"]');

    if (!newEmailInput || !submitButton) return;

    const newEmail = newEmailInput.value;

    
    submitButton.disabled = true;
    submitButton.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Submitting...';

    try {
        const response = await fetch('/api/request-email-change', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ newEmail })
        });

        const data = await response.json();

        if (response.ok) {
            showNotification('success', data.message);
            newEmailInput.value = '';

            
            setTimeout(loadUserPendingRequests, 1000);
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while submitting the email change request');
    } finally {
        
        submitButton.disabled = false;
        submitButton.innerHTML = '<i class="fas fa-envelope-open-text"></i> Request Email Change';
    }
}

async function handleUsernameChange(e) {
    e.preventDefault();
    const newUsernameInput = document.getElementById('new-username');
    const submitButton = e.target.querySelector('button[type="submit"]');

    if (!newUsernameInput || !submitButton) return;

    const newUsername = newUsernameInput.value;

    
    submitButton.disabled = true;
    submitButton.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Submitting...';

    try {
        const response = await fetch('/api/request-username-change', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ newUsername })
        });

        const data = await response.json();

        if (response.ok) {
            showNotification('success', data.message);
            newUsernameInput.value = '';

            
            setTimeout(loadUserPendingRequests, 1000);
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while submitting the username change request');
    } finally {
        
        submitButton.disabled = false;
        submitButton.innerHTML = '<i class="fas fa-user-edit"></i> Request Username Change';
    }
}

async function handleBlacklistRequest() {
    const requestBtn = document.getElementById('request-blacklist');

    if (!requestBtn) return;

    const confirmed = confirm('Are you sure you want to request to be blacklisted from the leaderboard?');

    if (!confirmed) return;

    
    requestBtn.disabled = true;
    requestBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Submitting...';

    try {
        const response = await fetch('/api/request-blacklist', {
            method: 'POST'
        });

        const data = await response.json();

        if (response.ok) {
            showNotification('success', data.message);

            
            setTimeout(loadUserPendingRequests, 1000);
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while submitting the blacklist request');
    } finally {
        
        requestBtn.disabled = false;
        requestBtn.innerHTML = '<i class="fas fa-user-shield"></i> Request to be Blacklisted';
    }
}

async function handleAccountDeletionRequest() {
    const requestBtn = document.getElementById('request-delete-account');

    if (!requestBtn) return;

    const confirmed = confirm('Are you sure you want to request account deletion? This action cannot be undone.');

    if (!confirmed) return;

    
    requestBtn.disabled = true;
    requestBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Submitting...';

    try {
        const response = await fetch('/api/request-account-deletion', {
            method: 'POST'
        });

        const data = await response.json();

        if (response.ok) {
            showNotification('success', data.message);

            
            setTimeout(loadUserPendingRequests, 1000);
        } else {
            showNotification('error', data.error);
        }
    } catch (error) {
        console.error('Error:', error);
        showNotification('error', 'An error occurred while submitting the account deletion request');
    } finally {
        
        requestBtn.disabled = false;
        requestBtn.innerHTML = '<i class="fas fa-user-times"></i> Request Account Deletion';
    }
}

async function loadUserPendingRequests() {
    const requestContainer = document.getElementById('user-pending-requests-container');

    if (!requestContainer) return;

    try {
        
        requestContainer.innerHTML = `
            <div class="loading-state">
                <i class="fas fa-circle-notch fa-spin"></i>
                <p>Loading your requests...</p>
            </div>
        `;

        const response = await fetch('/api/user-pending-requests');
        const requests = await response.json();

        
        requestContainer.innerHTML = '';

        if (!requests || requests.length === 0) {
            
            requestContainer.innerHTML = `
                <div class="empty-state">
                    <i class="fas fa-inbox"></i>
                    <p>You don't have any pending requests</p>
                </div>
            `;
            return;
        }

        
        const sortedRequests = requests.sort((a, b) => {
            const statusOrder = { pending: 0, approved: 1, rejected: 2 };
            return statusOrder[a.status] - statusOrder[b.status];
        });

        
        sortedRequests.forEach(request => {
            const requestElement = createRequestElement(request);
            requestContainer.appendChild(requestElement);
        });
    } catch (error) {
        console.error('Error loading user pending requests:', error);
        requestContainer.innerHTML = `
            <div class="error-state">
                <i class="fas fa-exclamation-circle"></i>
                <p>Error loading your requests. Please try again later.</p>
            </div>
        `;
    }
}

function createRequestElement(request) {
    const requestCard = document.createElement('div');
    requestCard.className = 'request-card';

    
    const requestHeader = document.createElement('div');
    requestHeader.className = 'request-header';

    
    const requestType = document.createElement('div');
    requestType.className = 'request-type';

    
    let icon = 'fa-question-circle';

    switch (request.type) {
        case 'leaderboard_name_change':
            icon = 'fa-trophy';
            break;
        case 'username_change':
            icon = 'fa-user-edit';
            break;
        case 'email_change':
            icon = 'fa-envelope';
            break;
        case 'blacklist':
            icon = 'fa-user-shield';
            break;
        case 'account_deletion':
            icon = 'fa-user-times';
            break;
    }

    requestType.innerHTML = `<i class="fas ${icon}"></i> ${getFriendlyRequestType(request.type)}`;

    
    const requestStatus = document.createElement('div');
    requestStatus.className = `request-status ${request.status}`;
    requestStatus.textContent = request.status.toUpperCase();

    requestHeader.appendChild(requestType);
    requestHeader.appendChild(requestStatus);

    
    const requestDetails = document.createElement('div');
    requestDetails.className = 'request-details';

    const detailsText = getRequestDetails(request);
    if (detailsText) {
        requestDetails.textContent = detailsText;
    }

    
    requestCard.appendChild(requestHeader);

    
    if (detailsText) {
        requestCard.appendChild(requestDetails);
    }

    
    if (request.request_date) {
        const requestDate = document.createElement('div');
        requestDate.className = 'request-date';

        const date = new Date(request.request_date);
        const formattedDate = date.toLocaleDateString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric'
        });

        requestDate.innerHTML = `<i class="far fa-calendar-alt"></i> Requested on ${formattedDate}`;
        requestCard.appendChild(requestDate);
    }

    
    if (request.admin_response) {
        const adminInfo = document.createElement('div');
        adminInfo.className = 'admin-info';
        adminInfo.innerHTML = `<i class="fas fa-comment-dots"></i> Admin: "${request.admin_response}"`;
        requestCard.appendChild(adminInfo);
    }

    return requestCard;
}

function getFriendlyRequestType(type) {
    switch (type) {
        case 'leaderboard_name_change':
            return 'Leaderboard Name Change';
        case 'blacklist':
            return 'Blacklist Request';
        case 'account_deletion':
            return 'Account Deletion';
        case 'username_change':
            return 'Username Change';
        case 'email_change':
            return 'Email Change';
        default:
            return 'Request';
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

function showNotification(type, message) {
    
    let notificationContainer = document.getElementById('notification-container');

    if (!notificationContainer) {
        notificationContainer = document.createElement('div');
        notificationContainer.id = 'notification-container';
        notificationContainer.style.position = 'fixed';
        notificationContainer.style.top = '20px';
        notificationContainer.style.right = '20px';
        notificationContainer.style.maxWidth = '400px';
        notificationContainer.style.zIndex = '1000';
        document.body.appendChild(notificationContainer);
    }

    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.style.padding = '12px 20px';
    notification.style.marginBottom = '10px';
    notification.style.borderRadius = '5px';
    notification.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.15)';
    notification.style.display = 'flex';
    notification.style.alignItems = 'center';
    notification.style.justifyContent = 'space-between';
    notification.style.opacity = '0';
    notification.style.transform = 'translateX(50px)';
    notification.style.transition = 'opacity 0.3s, transform 0.3s';

    switch (type) {
        case 'success':
            notification.style.backgroundColor = 'rgba(76, 175, 80, 0.95)';
            notification.style.borderLeft = '4px solid #43a047';
            break;
        case 'error':
            notification.style.backgroundColor = 'rgba(244, 67, 54, 0.95)';
            notification.style.borderLeft = '4px solid #e53935';
            break;
        case 'warning':
            notification.style.backgroundColor = 'rgba(255, 152, 0, 0.95)';
            notification.style.borderLeft = '4px solid #fb8c00';
            break;
        default:
            notification.style.backgroundColor = 'rgba(33, 150, 243, 0.95)';
            notification.style.borderLeft = '4px solid #1e88e5';
    }

    const content = document.createElement('div');
    content.style.color = '#fff';
    content.style.flex = '1';

    let icon = '';
    switch (type) {
        case 'success':
            icon = '<i class="fas fa-check-circle"></i>';
            break;
        case 'error':
            icon = '<i class="fas fa-times-circle"></i>';
            break;
        case 'warning':
            icon = '<i class="fas fa-exclamation-triangle"></i>';
            break;
        default:
            icon = '<i class="fas fa-info-circle"></i>';
    }

    content.innerHTML = `${icon} ${message}`;

    const closeBtn = document.createElement('button');
    closeBtn.innerHTML = '<i class="fas fa-times"></i>';
    closeBtn.style.backgroundColor = 'transparent';
    closeBtn.style.border = 'none';
    closeBtn.style.color = '#fff';
    closeBtn.style.marginLeft = '10px';
    closeBtn.style.cursor = 'pointer';
    closeBtn.style.fontSize = '16px';

    closeBtn.addEventListener('click', () => {
        notification.style.opacity = '0';
        notification.style.transform = 'translateX(50px)';

        setTimeout(() => {
            notification.remove();
        }, 300);
    });

    notification.appendChild(content);
    notification.appendChild(closeBtn);
    notificationContainer.appendChild(notification);
    
    setTimeout(() => {
        notification.style.opacity = '1';
        notification.style.transform = 'translateX(0)';
    }, 10);

    setTimeout(() => {
        notification.style.opacity = '0';
        notification.style.transform = 'translateX(50px)';

        setTimeout(() => {
            notification.remove();
        }, 300);
    }, 5000);
}

function highlightField(field) {
    field.style.borderColor = '#f44336';
    field.style.boxShadow = '0 0 0 2px rgba(244, 67, 54, 0.25)';

    field.addEventListener('input', function removeHighlight() {
        field.style.borderColor = '';
        field.style.boxShadow = '';
        field.removeEventListener('input', removeHighlight);
    });
}

document.addEventListener('DOMContentLoaded', () => {
    const style = document.createElement('style');
    style.textContent = `
        .loading-state, .error-state, .empty-state {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 2rem;
            background-color: rgba(28, 28, 46, 0.3);
            border-radius: 8px;
            width: 100%;
            text-align: center;
        }
        
        .loading-state i, .error-state i, .empty-state i {
            font-size: 2.5rem;
            margin-bottom: 1rem;
            opacity: 0.7;
        }
        
        .loading-state i {
            color: #4da6ff;
        }
        
        .error-state i {
            color: #f44336;
        }
        
        .empty-state i {
            color: #4da6ff;
            opacity: 0.6;
        }
        
        .loading-state p, .error-state p, .empty-state p {
            color: #b8b8b8;
            margin: 0;
        }
        
        .request-date {
            color: #818181;
            font-size: 0.85rem;
            margin-top: 0.75rem;
        }
    `;

    document.head.appendChild(style);
});
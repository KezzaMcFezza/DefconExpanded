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
//Last Edited 14-04-2025

import { customAlert, customConfirm } from '../main/popup.js';

// Shared state variable
let isEditingProfile = false;
let hasChangedData = false;
let eventListenersInitialized = false;

function initializeProfileEditing() {
    const editButton = document.getElementById('edit-profile-btn');
    const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
    const editableFields = document.querySelectorAll('.editable');
    const editableLists = document.querySelectorAll('.editable-list');

    if (editButton) {
        editButton.addEventListener('click', toggleEdit);
    }

    if (editButtonMobile) {
        editButtonMobile.addEventListener('click', toggleEdit);
    }

    function toggleEdit() {
        // If we're currently editing and clicking to save
        if (isEditingProfile) {
            const shouldSave = hasChangedData ? 
                confirm('Save your changes?') : 
                true; // If no changes, just exit edit mode without prompt
            
            if (shouldSave) {
                saveChanges();
            } else {
                // If user cancels saving, just revert to non-editing state without saving
                exitEditMode(false);
                return;
            }
        }

        // Toggle the editing state
        isEditingProfile = !isEditingProfile;
        hasChangedData = false;
        
        document.body.classList.toggle('editing');
        
        if (editButton) {
            editButton.classList.toggle('editing');
            editButton.innerHTML = isEditingProfile ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.toggle('editing');
            editButtonMobile.innerHTML = isEditingProfile ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        // Update overlay visibility immediately
        const profileBannerOverlay = document.getElementById('profile-banner-overlay');
        const profilePictureOverlay = document.getElementById('profile-picture-overlay');
        
        if (profileBannerOverlay) {
            profileBannerOverlay.style.display = isEditingProfile ? 'flex' : 'none';
        }
        
        if (profilePictureOverlay) {
            profilePictureOverlay.style.display = isEditingProfile ? 'flex' : 'none';
        }

        if (isEditingProfile) {
            setTimeout(enableEditing, 0);
        }
        
        editableFields.forEach(field => {
            if (isEditingProfile) {
                field.setAttribute('data-editing', 'true');
            } else {
                field.removeAttribute('data-editing');
            }
        });
    }

    function enableEditing() {
        editableFields.forEach(field => {
            field.contentEditable = true;
            
            field.classList.add('temp-class');
            void field.offsetHeight; 
            field.classList.remove('temp-class');
            
            // Track changes to know if we should prompt for saving
            field.addEventListener('input', () => {
                hasChangedData = true;
            });
        });

        editableLists.forEach(list => {
            // Remove any existing add item buttons before adding new ones
            const existingBtn = list.parentNode.querySelector('.add-item-btn');
            if (existingBtn) existingBtn.remove();
            
            const addItemBtn = document.createElement('button');
            addItemBtn.textContent = 'Add Item';
            addItemBtn.classList.add('add-item-btn');
            addItemBtn.addEventListener('click', (e) => {
                e.preventDefault();
                addListItem(list);
                hasChangedData = true;
            });
            list.parentNode.insertBefore(addItemBtn, list.nextSibling);

            list.querySelectorAll('li').forEach(item => {
                // Remove existing delete buttons before adding new ones
                const existingDeleteBtn = item.querySelector('.delete-item-btn');
                if (existingDeleteBtn) existingDeleteBtn.remove();
                addDeleteButton(item);
            });
        });
    }

    function addListItem(list) {
        const newItem = document.createElement('li');
        const textSpan = document.createElement('span');
        textSpan.contentEditable = true;
        textSpan.textContent = 'New item';
        textSpan.addEventListener('input', () => {
            hasChangedData = true;
        });
        newItem.appendChild(textSpan);
        addDeleteButton(newItem);
        list.appendChild(newItem);
    }

    function addDeleteButton(item) {
        const deleteBtn = document.createElement('button');
        deleteBtn.innerHTML = '<i class="fas fa-times"></i>';
        deleteBtn.classList.add('delete-item-btn');
        deleteBtn.addEventListener('click', () => {
            item.remove();
            hasChangedData = true;
        });
        item.appendChild(deleteBtn);
    }

    function exitEditMode(saveChangesMode = true) {
        isEditingProfile = false;
        hasChangedData = false;
        
        // Update UI state
        document.body.classList.remove('editing');
        
        if (editButton) {
            editButton.classList.remove('editing');
            editButton.innerHTML = '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.remove('editing');
            editButtonMobile.innerHTML = '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        // Update overlay visibility
        const profileBannerOverlay = document.getElementById('profile-banner-overlay');
        const profilePictureOverlay = document.getElementById('profile-picture-overlay');
        
        if (profileBannerOverlay) {
            profileBannerOverlay.style.display = 'none';
        }
        
        if (profilePictureOverlay) {
            profilePictureOverlay.style.display = 'none';
        }

        // Disable editing for all fields
        editableFields.forEach(field => {
            field.contentEditable = false;
            field.removeAttribute('data-editing');
        });

        // Clean up edit mode UI elements
        editableLists.forEach(list => {
            const addItemBtn = list.parentNode.querySelector('.add-item-btn');
            if (addItemBtn) addItemBtn.remove();
            list.querySelectorAll('.delete-item-btn').forEach(btn => btn.remove());
        });
    }

    async function saveChanges() {
        try {
            const currentUserResponse = await fetch('/api/current-user');
            const currentUserData = await currentUserResponse.json();
            const profileUsername = document.getElementById('profile-username-label').textContent;

            if (!currentUserData.user || currentUserData.user.username !== profileUsername) {
                await customAlert('You are not authorized to edit this profile.');
                location.reload();
                return;
            }

            const profileData = {
                discord_username: document.getElementById('discord-info').textContent,
                steam_id: document.getElementById('steam-info').textContent,
                bio: document.getElementById('bio-text').textContent,
                defcon_username: document.getElementById('defcon-username').textContent,
                years_played: document.getElementById('years-of-service').textContent,
                main_contributions: Array.from(document.getElementById('main-contributions-list').children).map(li => li.textContent.replace('X', '').trim()),
                guides_and_mods: Array.from(document.getElementById('guides-and-mods-list').children).map(li => li.textContent.replace('X', '').trim())
            };

            // Show loading indicator
            const loadingIndicator = document.createElement('div');
            loadingIndicator.classList.add('loading-overlay');
            loadingIndicator.innerHTML = '<div class="spinner"><i class="fas fa-circle-notch fa-spin"></i></div>';
            document.body.appendChild(loadingIndicator);

            const response = await fetch('/api/update-profile', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(profileData),
            });

            const result = await response.json();

            // Remove loading indicator
            if (document.body.contains(loadingIndicator)) {
                document.body.removeChild(loadingIndicator);
            }

            if (result.success) {
                await customAlert('Profile updated successfully!');
            } else {
                await customAlert('Failed to update profile. Please try again.');
            }
            
            // Exit edit mode
            exitEditMode(false); // Don't call saveChanges again
        } catch (error) {
            console.error('Error updating profile:', error);
            
            // Remove loading indicator if it exists
            const loadingIndicator = document.querySelector('.loading-overlay');
            if (loadingIndicator && document.body.contains(loadingIndicator)) {
                document.body.removeChild(loadingIndicator);
            }
            
            await customAlert('An error occurred while updating the profile.');
            
            // Exit edit mode
            exitEditMode(false);
        }
    }
}

function formatDurationProfile(minutes) {
    if (!minutes || isNaN(minutes)) return '0 min';

    if (minutes < 60) {
        return `${Math.round(minutes)} min`;
    } else {
        const hours = Math.floor(minutes / 60);
        const remainingMinutes = Math.round(minutes % 60);
        return `${hours} hr ${remainingMinutes} min`;
    }
}

function initializeProfileImageEditing() {
    // Check if we've already initialized the listeners to prevent duplicates
    if (eventListenersInitialized) return;
    eventListenersInitialized = true;
    
    // Check if Cropper.js is loaded, if not, load it
    if (!window.Cropper) {
        loadCropperJS();
    }

    const profilePictureContainer = document.getElementById('profile-picture-container');
    const profileBannerOverlay = document.getElementById('profile-banner-overlay');
    const profilePictureOverlay = document.getElementById('profile-picture-overlay');
    const profileBanner = document.getElementById('profile-banner');
    const imageUpload = document.getElementById('image-upload');
    
    let currentEditType = null; // 'profile' or 'banner'
    let cropper = null;

    // Use the cropper modal element already in HTML
    let cropperModal = document.getElementById('cropper-modal');
    
    // Initialize profile picture click handler
    if (profilePictureOverlay) {
        profilePictureOverlay.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            if (document.body.classList.contains('editing')) {
                currentEditType = 'profile';
                if (imageUpload) imageUpload.click();
            }
        });
    }

    // Initialize banner click handler
    if (profileBannerOverlay) {
        profileBannerOverlay.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            if (document.body.classList.contains('editing')) {
                currentEditType = 'banner';
                if (imageUpload) imageUpload.click();
            }
        });
    }

    // Handle image upload - only bind this once
    if (imageUpload) {
        imageUpload.addEventListener('change', function(e) {
            if (!e.target.files || !e.target.files[0]) return;
            
            const file = e.target.files[0];
            const reader = new FileReader();
            
            reader.onload = function(event) {
                const cropperImage = document.getElementById('cropper-image');
                cropperImage.src = event.target.result;
                
                // Open the modal
                cropperModal.style.display = 'flex';
                
                // Initialize Cropper.js
                if (cropper) {
                    cropper.destroy();
                }
                
                // Set different aspect ratio based on edit type
                const aspectRatio = currentEditType === 'profile' ? 1 : 3;
                
                // Wait for the image to load before initializing cropper
                cropperImage.onload = function() {
                    cropper = new Cropper(cropperImage, {
                        aspectRatio: aspectRatio,
                        viewMode: 1,
                        dragMode: 'move',
                        autoCropArea: 1,
                        restore: false,
                        modal: true,
                        guides: true,
                        highlight: true,
                        cropBoxMovable: true,
                        cropBoxResizable: true,
                        toggleDragModeOnDblclick: false,
                        minContainerWidth: 200,
                        minContainerHeight: 200,
                        ready() {
                            // Set up circular crop for profile pictures
                            if (currentEditType === 'profile') {
                                const containerData = cropper.getContainerData();
                                const cropBoxData = cropper.getCropBoxData();
                                const size = Math.min(containerData.width, containerData.height) * 0.8;
                                
                                cropper.setCropBoxData({
                                    left: (containerData.width - size) / 2,
                                    top: (containerData.height - size) / 2,
                                    width: size,
                                    height: size
                                });
                                
                                // Add circular mask for profile pictures
                                const cropBox = document.querySelector('.cropper-view-box');
                                if (cropBox) {
                                    cropBox.style.borderRadius = '50%';
                                    const face = document.querySelector('.cropper-face');
                                    if (face) {
                                        face.style.borderRadius = '50%';
                                    }
                                }
                            }
                        }
                    });
                };
            };
            
            reader.readAsDataURL(file);
            
            // Reset file input to allow selecting the same file again
            e.target.value = '';
        });
    }
    
    // Add event listeners to close and save buttons
    document.querySelector('.cropper-close-btn').addEventListener('click', closeCropperModal);
    document.getElementById('cropper-cancel-btn').addEventListener('click', closeCropperModal);
    document.getElementById('cropper-save-btn').addEventListener('click', saveCroppedImage);

    // Close modal function
    function closeCropperModal() {
        if (cropperModal) {
            cropperModal.style.display = 'none';
        }
        if (cropper) {
            cropper.destroy();
            cropper = null;
        }
    }

    // Save cropped image function
    function saveCroppedImage() {
        if (!cropper) return;

        // Get canvas with cropped image
        const canvas = cropper.getCroppedCanvas({
            width: currentEditType === 'profile' ? 480 : 1200,
            height: currentEditType === 'profile' ? 480 : 400,
            fillColor: '#000',
            imageSmoothingEnabled: true,
            imageSmoothingQuality: 'high',
        });
        
        // Convert canvas to blob
        canvas.toBlob(async (blob) => {
            try {
                // Create form data for upload
                const formData = new FormData();
                formData.append('image', blob, `${currentEditType}.png`);
                formData.append('type', currentEditType);
                
                // Upload image
                const response = await fetch('/api/upload-profile-image', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    // Cache-bust the URL to force reload
                    const cacheBustedUrl = `${result.imageUrl}?t=${new Date().getTime()}`;
                    
                    // Update UI with new image
                    if (currentEditType === 'profile') {
                        const profilePicture = document.getElementById('profile-picture');
                        if (profilePicture) {
                            profilePicture.src = cacheBustedUrl;
                        }
                    } else if (currentEditType === 'banner') {
                        const profileBanner = document.getElementById('profile-banner');
                        if (profileBanner) {
                            profileBanner.style.backgroundImage = `url(${cacheBustedUrl})`;
                        }
                    }
                    
                    // Close the modal on success
                    closeCropperModal();
                    
                    await customAlert(`${currentEditType === 'profile' ? 'Profile picture' : 'Banner'} updated successfully!`);
                } else {
                    await customAlert(`Failed to update ${currentEditType === 'profile' ? 'profile picture' : 'banner'}. Please try again.`);
                    // Close the modal on failure too
                    closeCropperModal();
                }
            } catch (error) {
                console.error('Error saving image:', error);
                await customAlert(`An error occurred while saving your ${currentEditType === 'profile' ? 'profile picture' : 'banner'}.`);
                
                // Always close the modal
                closeCropperModal();
                
                // Ensure any other hanging loading overlays are removed
                document.querySelectorAll('.loading-overlay').forEach(overlay => {
                    if (overlay.parentNode) {
                        overlay.parentNode.removeChild(overlay);
                    }
                });
            }
        }, 'image/png');
    }
}

// Load Cropper.js dynamically
function loadCropperJS() {
    // Load CSS
    const cropperCss = document.createElement('link');
    cropperCss.rel = 'stylesheet';
    cropperCss.href = 'https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.css';
    document.head.appendChild(cropperCss);
    
    // Load JS
    const cropperScript = document.createElement('script');
    cropperScript.src = 'https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.js';
    document.head.appendChild(cropperScript);
    
    cropperScript.onerror = () => {
        console.error('Failed to load Cropper.js');
        customAlert('Failed to load image editor. Please try again later.');
    };
}

export { 
    initializeProfileEditing, 
    initializeProfileImageEditing, 
    formatDurationProfile
};